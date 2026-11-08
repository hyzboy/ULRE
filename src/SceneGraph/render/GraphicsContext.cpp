#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKPhysicalDevice.h>
#include <hgl/graph/module/GraphModuleManager.h>
#include <hgl/graph/module/RenderPassManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/RenderTargetManager.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/SSBOBufferRegistry.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/module/EnvironmentManager.h>
#include <hgl/vk/VKBindlessTextureManager.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/asset/LoadedFileManager.h>
#include <hgl/asset/AssetCPUTaskPool.h>
#include <hgl/thread/ShutdownMode.h>
#include <hgl/graph/asset/AssetSourceMount.h>
#include <hgl/graph/asset/MaterialAsyncLoadLane.h>
#include <hgl/asset/AssetIndex.h>
#include <hgl/asset/AssetSource.h>
#include <hgl/io/InputStream.h>
#include <hgl/mtl/MaterialRecipeInherit.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/type/StdString.h>
#include <hgl/log/Log.h>

#include "../gizmo/GizmoInternal.h"

namespace hgl::graph
{
    GraphicsContext::GraphicsContext(VulkanDevice *dev)
        : device(dev)
    {
    }

    GraphicsContext::~GraphicsContext()
    {
        Shutdown();
    }

    // 全局地址表基址（pc_root 的根入口 addr_global_addresses 的取址来源）。
    // 定义放这里而不是头里：头文件只有 GlobalSSBOBufferRegistry 的前置声明。
    uint64_t GraphicsContext::GetGlobalAddressesAddress(uint32_t frame_slot) const
    {
        return global_ssbo_registry ? global_ssbo_registry->GetGlobalAddressesAddress(frame_slot) : 0;
    }

    TextureManager *GraphicsContext::GetTextureManager()
    {
        if (tex_manager)
            return tex_manager;
        if (!fallback_tex_manager_)
            fallback_tex_manager_ = new TextureManager(this);
        return fallback_tex_manager_;
    }

    bool GraphicsContext::LoadMaterialRecipeFile(const OSString &path)
    {
        if (!material_recipe_registry_.LoadFile(path, GetTextureManager()))
            return false;

        // 显式批量/手工装载会改变「哪些请求可满足」——整表作废，让此前的
        // 失败标记与新注册名在下一次查找时重新裁决。
        recipe_lookup_cache_.Clear();
        return true;
    }

    mtl::MaterialAsyncLoadLane *GraphicsContext::GetMaterialAsyncLoadLane()
    {
        if (!material_async_lane_)
        {
            mtl::MaterialAsyncLoadLane *created = new mtl::MaterialAsyncLoadLane();

            // 全局 CPU 池已就位 ⇒ 借用它，不再自建（线程数不再各池一份）。
            if (asset_cpu_pool_)
                created->AttachPool(asset_cpu_pool_);

            if (!created->EnsureStarted())
            {
                // 起不了线程不是致命错误：调用方会退回同步装载路径。
                delete created;
                return nullptr;
            }

            material_async_lane_ = created;

            // 文件投递走全局读盘底座（加载线程）；LFM 若尚未创建，稍后其
            // Start 处会再注入一次。
            material_async_lane_->AttachFileManager(loaded_file_manager_);

            // 车道只在「表里没有」时才投递：调用方常会先同步装一遍配方目录，
            // 没有去重就会把同一批配方在后台再解析一遍。
            material_async_lane_->SetDedupRegistries(&material_recipe_registry_,
                                                     &material_data_block_registry_);
        }

        return material_async_lane_;
    }

    int GraphicsContext::PumpMaterialAsyncLoadLane()
    {
        if (!material_async_lane_)
            return 0;

        return material_async_lane_->Pump(&material_recipe_registry_,
                                          &material_data_block_registry_,
                                          GetTextureManager());
    }

    bool GraphicsContext::IsMaterialAsyncLoadPending() const
    {
        return material_async_lane_ && !material_async_lane_->IsIdle();
    }

    bool GraphicsContext::LoadMaterialRecipeDirectory(
        const OSString &path,
        int *out_file_count,
        int *out_error_count)
    {
        if (!material_recipe_registry_.LoadDirectory(
                path, GetTextureManager(), out_file_count, out_error_count))
            return false;

        recipe_lookup_cache_.Clear();
        return true;
    }

    const mtl::MaterialRecipe *GraphicsContext::FindMaterialRecipe(
        const char *name) const
    {
        return material_recipe_registry_.FindByName(name);
    }

    const mtl::MaterialRecipeAsset *GraphicsContext::FindMaterialRecipeAsset(
        const char *name) const
    {
        return material_recipe_registry_.FindAssetByName(name);
    }

    namespace
    {
        /**
         * 「包内文件名检索」：在所有已挂载包裹的内存索引表里为请求串找到
         * 材质配方（`MaterialRecipe`）唯一的逻辑路径。
         *
         * 匹配优先级：逻辑路径全等 > 路径后缀匹配（请求是逻辑路径的尾段）>
         * 末段（短名）匹配。命中后拼出 `rcp:pack(<挂载名>):<逻辑路径>` ——
         * 物理扩展名（`.recipe.toml` / `.recipe.fb`）从不出现在引用里，
         * 由索引裁决当前该用哪个编码格式。
         */
        bool FindRecipeLogicalPathInPacks(const char *request, OSString &out_uri)
        {
            out_uri.Clear();

            ManagedArray<AnsiString> pack_names;
            if (ListMountedAssetSourceNames(pack_names) <= 0)
                return false;

            const AnsiString ansi_request(request);
            const int req_len = ansi_request.Length();
            if (req_len <= 0)
                return false;

            const U8String u8_request(reinterpret_cast<const u8char *>(ansi_request.c_str()),
                                      req_len);

            // 「文件名 = 配方名去掉最后一段」的例外兜底：语料里只有两处
            // `PBRSpheres.recipe.toml → name="PBRSpheres.Lit"`、
            // `TextureQuad.recipe.toml → name="TextureQuad.UnlitTexture"`
            // （见 doc/material-shader-naming-and-layout.md §5.3）。请求的完整末段
            // 匹配不上时，再用「去掉最后一个 `.` 段」的主干重试一次末段匹配。
            const AnsiString ansi_stripped = ansi_request.Left(ansi_request.FindRightChar('.'));
            U8String u8_stripped;
            const int stripped_len = ansi_stripped.Length();
            if (stripped_len > 0)
                u8_stripped = U8String(
                    reinterpret_cast<const u8char *>(ansi_stripped.c_str()), stripped_len);

            int best_kind = 0;   // 4=全等 3=后缀 2=末段 1=去尾段后的末段
            AnsiString best_pack;
            U8String best_logical;

            ManagedArray<asset::AssetIndexEntry> entries;
            for (int p = 0; p < pack_names.GetCount(); ++p)
            {
                asset::AssetSource *src = asset::GetSource(*pack_names[p]);
                if (!src)
                    continue;

                entries.Clear();
                if (src->GetEntriesByType(asset::AssetType::MaterialRecipe, entries) <= 0)
                    continue;

                for (int i = 0; i < entries.GetCount(); ++i)
                {
                    const U8String &logical = entries[i]->logical_path;
                    const int len = logical.Length();
                    if (len <= 0)
                        continue;

                    int kind = 0;
                    if (len == req_len && logical.Comp(0, u8_request) == 0)
                        kind = 4;
                    else if (len > req_len &&
                             logical.Comp(len - req_len, u8_request) == 0 &&
                             logical.c_str()[len - req_len - 1] == u8char('/'))
                        kind = 3;
                    else
                    {
                        const int slash = logical.FindRightChar(u8char('/'));
                        const int base = (slash < 0) ? 0 : slash + 1;
                        if (len - base == req_len && logical.Comp(base, u8_request) == 0)
                            kind = 2;
                        else if (stripped_len > 0 &&
                                 len - base == stripped_len &&
                                 logical.Comp(base, u8_stripped) == 0)
                            kind = 1;
                    }

                    if (kind > best_kind)
                    {
                        best_kind = kind;
                        best_pack = *pack_names[p];
                        best_logical = logical;
                    }
                }
            }

            if (best_kind == 0)
                return false;

            out_uri = OSString(OS_TEXT("rcp:pack(")) + ToOSString(best_pack.c_str())
                    + OS_TEXT("):")
                    + ToOSString(reinterpret_cast<const char *>(best_logical.c_str()));
            return true;
        }
    }//anonymous namespace

    bool GraphicsContext::LoadMaterialRecipeFromURI(const OSString &uri_text,
                                                    AnsiString &out_recipe_name)
    {
        out_recipe_name.Clear();

        AnsiString err;
        io::InputStream *stream = OpenAssetInputStream(uri_text,
                                                       asset::AssetType::MaterialRecipe,
                                                       &err);
        if (!stream)
        {
            GLogError(u8"[GraphicsContext] 配方取流失败: uri=%ls reason=%s",
                      uri_text.c_str(), err.c_str());
            return false;
        }

        // `LoadFromStream` 不消费流的所有权（契约见其声明），这里负责 delete。
        const bool ok = material_recipe_registry_.LoadFromStream(
            stream, uri_text.c_str(), GetTextureManager(), &out_recipe_name);
        delete stream;

        if (!ok && out_recipe_name.IsEmpty())
        {
            GLogError(u8"[GraphicsContext] 配方解析/注册失败: uri=%ls", uri_text.c_str());
            return false;
        }

        if (!ok)
        {
            // 重名撞上已存在的句柄：名字照样可用，调用方按名继续查即可。
            GLogWarning(u8"[GraphicsContext] 配方已注册（重名命中已有句柄）: uri=%ls recipe=%s",
                        uri_text.c_str(), out_recipe_name.c_str());
        }

        return true;
    }

    bool GraphicsContext::LocateMaterialRecipeURI(const char *request, OSString &out_uri)
    {
        out_uri.Clear();

        // 带 scheme（`rcp:` / `file:`）或盘符 ⇒ 整个串交给 URI 分派器解读。
        if (strchr(request, ':') != nullptr)
        {
            out_uri = ToOSString(request);
            return true;
        }

        // 否则走包裹的文件名检索系统（短名与包内相对路径都在这一次内存扫描里解决）。
        if (FindRecipeLogicalPathInPacks(request, out_uri))
            return true;

        // 检索不到但形如相对路径 ⇒ 保留「直接给物理路径」的老写法：按本地文件开流
        // （`OpenAssetInputStream` 对裸路径分派）。短名不走这条，避免误 hit 错文件。
        if (strchr(request, '/') != nullptr || strchr(request, '\\') != nullptr)
        {
            out_uri = ToOSString(request);
            return true;
        }

        return false;
    }

    const mtl::MaterialRecipe *GraphicsContext::ResolveMaterialRecipe(const char *request,
                                                                      int depth)
    {
        if (!request || !request[0])
        {
            GLogError(u8"[GraphicsContext] 无法查找材质配方：名称为空");
            return nullptr;
        }

        if (const mtl::MaterialRecipe *recipe = material_recipe_registry_.FindByName(request))
            return recipe;

        const AnsiString key(request);

        AnsiString registered;
        if (const AnsiString *cached = recipe_lookup_cache_.GetValuePointer(key))
        {
            if (cached->IsEmpty())
                return nullptr;   // 已确认取不到，不再重试取流

            registered = *cached;
            if (const mtl::MaterialRecipe *recipe =
                    material_recipe_registry_.FindByName(registered.c_str()))
                return recipe;
        }
        else
        {
            OSString uri_text;
            if (!LocateMaterialRecipeURI(request, uri_text))
            {
                GLogError(u8"[GraphicsContext] 材质配方未找到: recipe=%s registered=%d",
                          request, material_recipe_registry_.GetCount());
                recipe_lookup_cache_.Add(key, AnsiString());
                return nullptr;
            }

            AnsiString name;
            if (!LoadMaterialRecipeFromURI(uri_text, name))
            {
                recipe_lookup_cache_.Add(key, AnsiString());
                return nullptr;
            }

            registered = name;
            recipe_lookup_cache_.Add(key, registered);

            if (const mtl::MaterialRecipe *recipe =
                    material_recipe_registry_.FindByName(registered.c_str()))
            {
                GLogDebug(u8"[GraphicsContext] 材质配方已按需加载: request=%s uri=%ls domain=%s",
                          request, uri_text.c_str(),
                          mtl::GetMaterialDomainName(recipe->domain));
                return recipe;
            }
        }

        // 到这里：句柄在表里但没到 `Ready` ⇒ 它的 `parent` 还没就绪。按需补加载
        // 父配方；父注册时触发的 `ResolvePending` 会把本条推进到 `Ready`。
        if (depth >= 8)
        {
            GLogError(u8"[GraphicsContext] 配方父链过深（疑似成环）: recipe=%s",
                      registered.c_str());
            recipe_lookup_cache_.GetValueRef(key) = AnsiString();
            return nullptr;
        }

        const mtl::MaterialRecipeAsset *asset =
            material_recipe_registry_.FindAssetByName(registered.c_str());
        if (!asset)
        {
            // 句柄在等待期间被摘除（校验失败 / 父链成环收尾）⇒ 本请求不可满足。
            GLogError(u8"[GraphicsContext] 配方句柄已摘除: recipe=%s", registered.c_str());
            recipe_lookup_cache_.GetValueRef(key) = AnsiString();
            return nullptr;
        }

        const AnsiString &parent_text = asset->GetSpec().parent;
        mtl::MaterialRecipeParentRef parent_ref;
        if (parent_text.IsEmpty() ||
            !mtl::ParseMaterialRecipeParentRef(parent_text.c_str(), parent_ref) ||
            parent_ref.IsNone())
        {
            GLogError(u8"[GraphicsContext] 配方停在等待态但父引用不可解析: recipe=%s parent=%s",
                      registered.c_str(), parent_text.c_str());
            recipe_lookup_cache_.GetValueRef(key) = AnsiString();
            return nullptr;
        }

        bool parent_loaded = false;
        if (parent_ref.kind == mtl::MaterialRecipeParentRefKind::ByName)
        {
            parent_loaded = ResolveMaterialRecipe(parent_ref.name.c_str(), depth + 1) != nullptr;
        }
        else
        {
            AnsiString parent_name;
            if (LoadMaterialRecipeFromURI(parent_ref.path, parent_name) &&
                !parent_name.IsEmpty())
            {
                const mtl::MaterialRecipeAsset *parent_asset =
                    material_recipe_registry_.FindAssetByName(parent_name.c_str());
                parent_loaded = parent_asset && parent_asset->IsReady();
            }
        }

        if (const mtl::MaterialRecipe *recipe =
                material_recipe_registry_.FindByName(registered.c_str()))
            return recipe;

        GLogError(u8"[GraphicsContext] 材质配方父链无法解析: recipe=%s parent=%s parent_loaded=%d",
                  registered.c_str(), parent_text.c_str(), parent_loaded ? 1 : 0);
        recipe_lookup_cache_.GetValueRef(key) = AnsiString();
        return nullptr;
    }

    const mtl::MaterialRecipe *GraphicsContext::FindOrLoadMaterialRecipe(const char *name)
    {
        return ResolveMaterialRecipe(name, 0);
    }

    bool GraphicsContext::Initialize()
    {
        if (!device)
            return false;

        // Create module manager
        module_manager = new GraphModuleManager();
        if (!module_manager)
            return false;

        // Initialize all managers through module system
        rp_manager = module_manager->GetOrCreate<RenderPassManager>();
        if (!rp_manager)
            return false;

        tex_manager = module_manager->GetOrCreate<TextureManager>();
        if (!tex_manager)
            return false;

        // 无设备期用过的兜底 TextureManager 必须连同它名下的配方句柄一起退役：
        // `MaterialRecipe::textures[].asset` 是裸观察指针，句柄表随 fallback 销毁后
        // 这些指针即悬垂。清空注册器 + 清空查找缓存 ⇒ 下一次 lookup 会用真设备的
        // TextureManager 重新按需读一遍所需配方（默认目录之外按需手工加载的配方会被
        // 丢弃，这是从"无设备"进入"有设备"时唯一安全的取舍）。
        if (fallback_tex_manager_)
        {
            material_recipe_registry_.Clear();
            recipe_lookup_cache_.Clear();
            SAFE_CLEAR(fallback_tex_manager_)
        }

        sampler_manager = module_manager->GetOrCreate<SamplerManager>();
        if (!sampler_manager)
            return false;

        material_manager = module_manager->GetOrCreate<ShaderProgramManager>();
        if (!material_manager)
            return false;

        buffer_manager = module_manager->GetOrCreate<BufferManager>();
        if (!buffer_manager)
            return false;

        resource_domain_manager = module_manager->GetOrCreate<SSBOBufferRegistry>();
        if (!resource_domain_manager)
            return false;

        global_ssbo_registry = module_manager->GetOrCreate<GlobalSSBOBufferRegistry>();
        if (!global_ssbo_registry)
            return false;

        // GeometryManager 必须在 GlobalSSBOBufferRegistry 之后创建：GraphModuleManager
        // 按创建逆序释放，几何体析构时会向 MeshDrawParams 池归还行号，要求池仍存活。
        geometry_manager = module_manager->GetOrCreate<GeometryManager>();
        if (!geometry_manager)
            return false;

        // 几何资产句柄表：绑定设备与缓冲管理器，此后 `GetOrLoad` 即可直接用。
        geometry_asset_registry_.SetTargets(device, buffer_manager);
        geometry_asset_registry_ready_ = true;

        // 图元资产绑定表：几何来自上面的句柄表，配方来自本上下文的配方注册器。
        primitive_asset_registry_.SetSources(&geometry_asset_registry_,
                                             &material_recipe_registry_);
        primitive_asset_registry_ready_ = true;

        // 异步场景句柄表：绑定设备与几何管理器，并登记为进程内默认表，
        // 使 `RequestModelAsset(uri)` 无需先摸到本上下文。
        model_asset_registry_.SetTargets(device, geometry_manager);
        SetDefaultModelAssetRegistry(&model_asset_registry_);

        // 环境综合信息统一管理（sky 等）。必须在 BufferManager 之后创建：
        // default profile 在模块获得 GraphicsContext 时立即物化 sky UBO，
        // 保证任何 world（含离屏 RenderOnce）第一帧即拿到有效环境数据。
        env_manager = module_manager->GetOrCreate<EnvironmentManager>();
        if (!env_manager)
            return false;

        bindless_texture_manager_ = new BindlessTextureManager();
        if (!bindless_texture_manager_)
            return false;

        if (!bindless_texture_manager_->Init(device))
            return false;

        // 统一 Sampler 注册：解析 contract/sampler.toml → 按序 vkCreateSampler 写 binding=1。
        // 此处与 ShaderGen 的 SamplerPresetLibrary 同源，保证 name→index 唯一一致。
        {
            const hgl::filesystem::Path sampler_toml =
                hgl::filesystem::Path(ToOSString(mtl::GetShaderLibraryPath()))
                / OSString(OS_TEXT("contract/sampler.toml"));

            auto &preset_lib = mtl::SamplerPresetLibrary::Instance();
            if (preset_lib.Load(sampler_toml.ToOSString()))
            {
                const uint32 count = preset_lib.GetCount();
                if (count > 0
                    && !bindless_texture_manager_->RegisterSamplers(
                        preset_lib.GetCreateInfo(0), count))
                {
                    GLogError("[GraphicsContext] RegisterSamplers failed");
                    return false;
                }
            }
            else
            {
                GLogWarning("[GraphicsContext] contract/sampler.toml 加载失败，sampler 将不可用");
            }
        }

        material_manager->SetBindlessLayout(bindless_texture_manager_->GetLayout());

        // 全局共享 CPU 工作池（Tier 2：解析/编译等纯 CPU 阶段）。材质车道及后续
        // 消费者借用它，不再各建各的池。先于读盘底座创建：读盘完成回调会把
        // 解析任务投进这里，池必须先于投递方存活。
        asset_cpu_pool_ = new asset::AssetCPUTaskPool();

        if (!asset_cpu_pool_->Start(2))
        {
            // 起不了共享池不致命：消费者退回自建池路径。
            GLogWarning("[GraphicsContext] AssetCPUTaskPool Start failed; consumers will self-own pools");
            SAFE_CLEAR(asset_cpu_pool_)
        }

        // 全局异步文件加载底座：后台单 IO 线程（同一物理设备串行读盘更稳），
        // 主线程侧由各消费系统在自己的更新阶段调用 Pump()/Collect()。
        // 必须在任何消费者（纹理流式加载系统等）创建之前就位。
        loaded_file_manager_ = new LoadedFileManager();

        if (!loaded_file_manager_ || !loaded_file_manager_->Start(1))
        {
            GLogError("[GraphicsContext] LoadedFileManager Start failed");
            return false;
        }

        // 材质车道可能比 LFM 更早创建（懒启动）：此时补注入，保证文件投递碰到盘。
        if (material_async_lane_)
            material_async_lane_->AttachFileManager(loaded_file_manager_);

        // 同理补注入共享 CPU 池；若车道已自建池，`AttachPool` 会告警并维持现状
        //（功能不受影响，只是多养两个线程）。
        if (material_async_lane_ && asset_cpu_pool_)
            material_async_lane_->AttachPool(asset_cpu_pool_);

        // 资产包裹挂载的正常路径在 AppFramework::Init（读 asset_mount.toml，
        // 进程级、与图形无关）。这里只做兜底：无框架参与（如纯引擎测试）时
        // 补挂默认 res；同名挂载幂等跳过，绝不会覆盖框架已挂好的表。
        // 注销同样归框架退出路径（AppFramework 析构 → UnmountAllAssetSources）。
        if (GetMountedAssetSource() == nullptr)
        {
            AnsiString mount_error;

            if (!MountDefaultAssetSources(&mount_error) && !mount_error.IsEmpty())
                GLogWarning("[GraphicsContext] 默认资产包裹挂载失败：%s", mount_error.c_str());
        }

        // Scene 集（Set 0）已整体退场（S3）：viewport/sky/shadow 全走 BDA，
        // 设备级全局集只剩 Bindless 一个（见 SetSceneLayout 的删除）。

        // Set graphics context for module manager
        module_manager->SetGraphicsContext(this);

        if (!global_ssbo_registry->IsInitialized())
        {
            GLogError(
                "[GraphicsContext] GlobalSSBOBufferRegistry initialization failed");
            return false;
        }

        return true;
    }

    void GraphicsContext::Shutdown()
    {
        if (device)
            device->WaitIdle();

        // Gizmo 驻留资源(VDM/GeometryCreater/颜色行租约)挂在文件级 static 上,
        // 无人释放会在 static 析构(晚于本函数)时把行还回已销毁的
        // GlobalSSBOBufferRegistry 行池 —— 必须趁设备与模块还活着时释放。
        ForceReleaseGizmoSystemResources();

        level_asset_registry_.Clear();

        // 异步场景句柄排在几何 / 纹理注册器之前清空：句柄只是状态机 + 计划单，
        // 但它的观察指针不得活过下面的 GPU 资源回收。同时注销进程内默认表。
        SetDefaultModelAssetRegistry(nullptr);
        model_asset_registry_.Clear();

        // 先收掉后台加载底座：Stop(Drain) 会等所有已提交读盘结束并同步派发取消回调，
        // 保证此后不再有 IO 线程访问本上下文持有的任何资源。必须早于模块销毁。
        if (loaded_file_manager_)
        {
            loaded_file_manager_->Stop();
            SAFE_CLEAR(loaded_file_manager_)
        }

        // 包裹注销已上收到框架退出路径（AppFramework 析构），必须晚于此处
        // LoadedFileManager 停稳 —— 时序由框架保证，本上下文不再代管。

        // 图元资产绑定表持有几何/配方注册器的裸指针 ⇒ 必须先于它们清空。
        if (primitive_asset_registry_ready_)
        {
            primitive_asset_registry_.Clear();
            primitive_asset_registry_.SetSources(nullptr, nullptr);
            primitive_asset_registry_ready_ = false;
        }

        // 材质装载车道必须先停：它在工作线程里解析配方/数据块，成品最终由主线程
        // 的 Pump() 写进下面两张注册表。Stop(Drain) 后补泵一次，把已完成的成品
        // 落表，再解除挂接，此后不再有任何线程访问这两张表。
        if (material_async_lane_)
        {
            material_async_lane_->Shutdown(&material_recipe_registry_,
                                           &material_data_block_registry_,
                                           GetTextureManager());
            delete material_async_lane_;
            material_async_lane_ = nullptr;
        }

        // 共享 CPU 池最后停：读盘底座与车道的收尾都可能仍需要它执行在途任务。
        // `Stop(Drain)` 幂等（车道若自持池已停过自己的，这里只管本上下文这一份）。
        if (asset_cpu_pool_)
        {
            asset_cpu_pool_->Stop(thread::ShutdownMode::Drain);
            SAFE_CLEAR(asset_cpu_pool_)
        }

        // Recipe data contains TextureAsset observers owned by TextureManager.
        // Drop the cache before destroying the module manager that owns them.
        material_recipe_registry_.Clear();
        recipe_lookup_cache_.Clear();

        SAFE_CLEAR(fallback_tex_manager_)

        // 几何资产句柄拥有各自的 Geometry，几何析构会向 MeshDrawParams 池归还行号，
        // 因此必须在模块（含 GlobalSSBOBufferRegistry / GeometryManager）销毁之前清空。
        if (geometry_asset_registry_ready_)
        {
            geometry_asset_registry_.Clear();
            geometry_asset_registry_.SetTargets(nullptr, nullptr);
            geometry_asset_registry_ready_ = false;
        }

        // 材质数据块的句柄持有全局池的行区间租约，析构时要把行号还回去
        // ⇒ 必须在模块（含 GlobalSSBOBufferRegistry）销毁之前清空。
        material_data_block_registry_.Clear();

        std::cout << "[DEBUG] GraphicsContext::Shutdown() - Deleting GraphModuleManager" << std::endl;
        // GraphModuleManager destructor will call Release() on all modules automatically
        // This ensures proper cleanup order
        SAFE_CLEAR(module_manager)
        std::cout << "[DEBUG] GraphicsContext::Shutdown() - GraphModuleManager deleted" << std::endl;

        // Clear all manager pointers (they're owned by module_manager)
        rp_manager = nullptr;
        tex_manager = nullptr;
        rt_manager = nullptr;
        material_manager = nullptr;
        buffer_manager = nullptr;
        sampler_manager = nullptr;
        geometry_manager = nullptr;
        resource_domain_manager = nullptr;
        global_ssbo_registry = nullptr;
        env_manager = nullptr;

        SAFE_CLEAR(bindless_texture_manager_)
    }

    void GraphicsContext::OnResize(const VkExtent2D &extent)
    {
        if (!module_manager)
            return;

        // 通知所有模块窗口大小改变
        if (rp_manager) rp_manager->OnResize(extent);
        if (tex_manager) tex_manager->OnResize(extent);
        if (rt_manager) rt_manager->OnResize(extent);
        if (material_manager) material_manager->OnResize(extent);
        if (buffer_manager) buffer_manager->OnResize(extent);
        if (sampler_manager) sampler_manager->OnResize(extent);
        if (geometry_manager) geometry_manager->OnResize(extent);
    }

    VulkanDevAttr *GraphicsContext::GetDevAttr() const
    {
        return device ? device->GetDevAttr() : nullptr;
    }

    VulkanPhyDevice *GraphicsContext::GetPhyDevice() const
    {
        return device ? const_cast<VulkanPhyDevice *>(device->GetPhyDevice()) : nullptr;
    }

    const mtl::contract::PhysicalDeviceProfileLite *GraphicsContext::GetPhysicalDeviceProfile() const
    {
        auto *pd = GetPhyDevice();
        return pd ? &pd->GetPhysicalDeviceProfile() : nullptr;
    }

    VkDevice GraphicsContext::GetVkDevice() const
    {
        return device ? device->GetDevice() : VK_NULL_HANDLE;
    }

    static void BindGlobalDescriptorsInternal(VkCommandBuffer cmd_buf,
                                              VkPipelineLayout layout,
                                              VkPipelineBindPoint bind_point,
                                              VulkanDevice *device,
                                              BindlessTextureManager *bindless_mgr)
    {
        auto *attr = device ? device->GetDevAttr() : nullptr;
        const bool use_desc_buffer = device && device->IsDescriptorBufferActive();

        if (use_desc_buffer && attr && attr->cmd_bind_descriptor_buffers)
        {
            uint32_t bindless_buffer_index = 0;
            VkDescriptorBufferBindingInfoEXT binding_infos[2]{};
            uint32_t buffer_count = 0;

            if (bindless_mgr && bindless_mgr->IsValid())
            {
                bindless_buffer_index = buffer_count;
                auto &b = binding_infos[buffer_count++];
                b.sType   = VK_STRUCTURE_TYPE_DESCRIPTOR_BUFFER_BINDING_INFO_EXT;
                b.pNext   = nullptr;
                b.address = bindless_mgr->GetDescriptorBufferAddress();
                b.usage   = VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT
                          | VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT;
            }

            if (buffer_count > 0)
            {
                attr->cmd_bind_descriptor_buffers(cmd_buf, buffer_count, binding_infos);
            }

            if (bindless_mgr && bindless_mgr->IsValid())
            {
                bindless_mgr->BindOffsetToCmd(cmd_buf,
                                              layout,
                                              static_cast<uint32_t>(graph::DescriptorSetType::Bindless),
                                              bindless_buffer_index,
                                              bind_point);
            }
        }
        else
        {
            // 传统 DescriptorPool 双轨回退路径 (如 RenderDoc 环境或不支持 DescriptorBuffer 的设备)
            if (bindless_mgr && bindless_mgr->IsValid())
            {
                bindless_mgr->BindToCmd(cmd_buf,
                                        layout,
                                        static_cast<uint32_t>(graph::DescriptorSetType::Bindless),
                                        bind_point);
            }
        }
    }

    void GraphicsContext::BindGlobalDescriptorSets(RenderCmdBuffer *cmd, VkPipelineLayout layout)
    {
        if (!cmd || cmd->global_sets_bound)
            return;

        BindGlobalDescriptorsInternal(*cmd, layout, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                      device, GetBindlessTextureManager());

        cmd->global_sets_bound = true;
    }

    void GraphicsContext::BindGlobalDescriptorSets(ComputeCmdBuffer *cmd, VkPipelineLayout layout)
    {
        if (!cmd || cmd->global_sets_bound)
            return;

        BindGlobalDescriptorsInternal(*cmd, layout, VK_PIPELINE_BIND_POINT_COMPUTE,
                                      device, GetBindlessTextureManager());

        cmd->global_sets_bound = true;
    }

} // namespace hgl::graph
