#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKObjectNameBuilder.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKShaderModuleMap.h>
#include<hgl/common/ShaderStageDef.h>
// 生成器开关（`#if ULRE_ENABLE_SHADERGEN`）的唯一定义处：ON 时头内默认 1，
// OFF 时 CMake 全局下发 0。必须显式包含，不能靠间接包含 —— 否则开发形态会
// 误判为 0，把 compute 的现编路径整条摘掉。
#include<hgl/common/RenderOptions.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramCreatePrecheckAdapter.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/ShaderBuildContext.h>
#include<hgl/mtl/ShaderCreateInfo.h>
#include<hgl/mtl/ShaderArtifactStore.h>
#include<hgl/mtl/ShaderComputeProgramManifest.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/ShaderProgramBuildBackend.h>
#include<hgl/mtl/ShaderProgramManifestBackend.h>
#include<hgl/mtl/ShaderProgramManifestKey.h>
#include<hgl/mtl/MaterialDefinitionFile.h>
#include<hgl/mtl/SceneRenderTemplateResolver.h>
#include<hgl/mtl/ShaderCacheRoot.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/utf.h>
#include<cstdint>
#include<vector>
#include<cstdlib>
#include<cwchar>

namespace hgl::graph{

namespace
{
    bool ResolveMaterialDefinitionForRequest(const mtl::MaterialDefinitionBuildRequest &request,
                                             mtl::MaterialDefinition &out_bmi)
    {
        const std::string &mtl_def_id = request.recipe.mtl_def_id;

        if (mtl::TryGetMaterialDefinitionByID(mtl_def_id, out_bmi))
            return true;

        // 显式诊断：材质 ID 配置错误不应静默消失（fallback 只是运行时安全网）
        GLogError(u8"[ShaderProgramManager] material definition not found: %s — falling back to %s",
                  mtl_def_id.c_str(), mtl::GetFallbackMaterialDefinitionID());

        return mtl::TryGetMaterialDefinitionByID(
            mtl::GetFallbackMaterialDefinitionID(), out_bmi);
    }

    bool ResolveRuntimeRenderTemplateRequest(
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialDefinitionBuildRequest &request,
        mtl::RenderTemplateRequest &out_request)
    {
        (void)request;
        if (out_request.template_id == mtl::RenderTemplateID::Unknown)
        {
            GLogError(
                "[ShaderProgramManager] Render template request is not caller-selected: id=%s",
                definition.definition_id.c_str());
            return false;
        }

        if (mtl::AppendMaterialRenderTemplateRoots(definition, out_request))
            return true;

        GLogError(
            "[ShaderProgramManager] Render template request completion failed: id=%s",
            definition.definition_id.c_str());
        return false;
    }

    void CreateShaderStageList(ValueArray<VkPipelineShaderStageCreateInfo> &shader_stage_list,ShaderModuleMap *shader_maps)
    {
        const ShaderModule *sm;

        const int shader_count=shader_maps->GetCount();
        shader_stage_list.Resize(shader_count);

        VkPipelineShaderStageCreateInfo *p=shader_stage_list.GetData();

        for(auto [stage, module] : *shader_maps)
        {
            sm = module;
            mem_copy(p,sm->GetCreateInfo(),1);

            ++p;
        }
    }

    bool BuildShaderModulesFromCreateInfoMap(ShaderProgramManager *manager,
                                             const AnsiString &mtl_name,
                                             const mtl::ShaderCreateInfoMap &sci_map,
                                             const mtl::ShaderBuildContext *build_spec,
                                             ShaderModuleMap *shader_maps,
                                             ShaderProgram *mtl)
    {
        if (!manager || !shader_maps)
            return false;

        if (sci_map.GetCount() < 2)
            return false;

        for (auto [stage, sci_ptr] : sci_map)
        {
            (void)stage;

            if (!sci_ptr)
                return false;

            const ShaderModule *module = nullptr;
            mtl::ShaderArtifactStore *artifact_store =
                build_spec ? build_spec->GetArtifactStore() : nullptr;
            const mtl::ShaderStageKey *cache_key = nullptr;
            bool artifact_loaded = false;
            if (build_spec && build_spec->HasProgramLink())
            {
                const auto &link = build_spec->GetProgramLink();
                cache_key = stage == ShaderStage::Mesh
                    ? &link.mesh_stage : stage == ShaderStage::Fragment
                        ? &link.fragment_stage : nullptr;
            }

            if (artifact_store && cache_key)
            {
                hgl::ValueArray<hgl::uint8> cached_spv;
                if (artifact_store->LoadStageSPV(*cache_key, cached_spv))
                {
                    artifact_loaded = true;
                    module = manager->CreateShaderModuleFromSPV(
                        *cache_key,
                        reinterpret_cast<const uint32_t *>(cached_spv.GetData()),
                        static_cast<size_t>(cached_spv.GetCount()));
                    if (!module)
                        GLogError(u8"[ShaderProgramManager] failed to create Vulkan shader module from SPV: %s",
                                  cache_key->ToString().c_str());
                }
            }

            if (!module)
            {
                if (artifact_store
                 && artifact_store->GetCacheMode() == mtl::ShaderCacheMode::ReadOnly)
                {
                    if (artifact_loaded)
                    {
                        GLogError(u8"[ShaderProgramManager] readonly SPV artifact is invalid or incompatible: %s",
                                  cache_key ? cache_key->ToString().c_str() : "<unknown>");
                    }
                    else
                    {
                        GLogError(u8"[ShaderProgramManager] readonly shader artifact missing: %s",
                                  cache_key ? cache_key->ToString().c_str() : "<unknown>");
                    }

                    return false;
                }
                module = manager->CreateShaderModule(mtl_name, sci_ptr);
            }
            if (!module)
                return false;

            shader_maps->Add(module);
        }

        return true;
    }

    // 进程级单例（与 GetMaterialDefinitionFileRegistry 同一模式）。
    // 模式由环境变量 ULRE_SHADER_CACHE_MODE 控制：
    //   readonly —— 只读（发布形态：只接受缓存目录里已有的 SPV 产物，
    //              缓存 miss 即材质构建失败，用于验证缓存覆盖完整性）；
    //   默认 BuildIfMissing（开发形态：命中免编译，未命中编译后回填）。
    // 缓存 key 含 GLSL 源码/资源契约/编译器 profile 哈希，任何输入变化自然
    // miss 并覆盖旧条目；损坏文件因 header/payload 哈希校验失败按 miss 处理。
    // ReadOnly 的 miss 硬失败（FinalizeShaderBuildContext 返回 false）是设计
    // 行为——发布前必须先把全部变体的 SPV 写入缓存目录。
    mtl::ShaderArtifactStore *GetRuntimeShaderArtifactStore()
    {
        static const OSString cache_root = mtl::GetShaderCacheRootPath();
        static const bool readonly_mode = []()
        {
            const wchar_t *mode = _wgetenv(L"ULRE_SHADER_CACHE_MODE");
            return mode
                && (wcscmp(mode, L"readonly") == 0
                 || wcscmp(mode, L"ro") == 0);
        }();
        static mtl::ShaderArtifactStore store(
            cache_root,
            readonly_mode
                ? mtl::ShaderCacheMode::ReadOnly
                : mtl::ShaderCacheMode::BuildIfMissing);
        static bool logged = false;
        if (!logged)
        {
            logged = true;
            if (cache_root.IsEmpty())
            {
                GLogWarning(u8"[ShaderProgramManager] shader SPV cache disabled: cannot resolve cache root");
            }
            else
            {
                const U8String root_utf8 = ToU8String(cache_root);
                GLogInfo(u8"[ShaderProgramManager] shader SPV cache root=%s (mode=%s)",
                         root_utf8.c_str(),
                         readonly_mode ? "ReadOnly" : "BuildIfMissing");
            }
        }
        return &store;
    }

}//namespace

bool SelectCurrentSceneRenderTemplateRequest(
    const mtl::MaterialDefinition &definition,
    const mtl::MaterialDefinitionBuildRequest &request,
    mtl::RenderTemplateRequest &out_request)
{
    // 路由与 ambient 覆盖只有一份实现(ShaderGen),运行时与离线回归共用,
    // 避免 SceneGraph / 工具 / 测试各自复制判定逻辑而逐渐分叉。
    // ambient provider 由环境层给（request.ambient_module_name），不是材质数据。
    return mtl::SelectSceneRenderTemplateRequest(
        definition, request.shader_program_purpose, request.recipe,
        out_request, request.ambient_module_name);
}

GRAPH_MODULE_CONSTRUCT(ShaderProgramManager)
{
    (void)mtl::GetMaterialDefinitionFileRegistry();
}

void ShaderProgramManager::Release()
{
    // 清理所有材质
    if (rm_material.GetCount() > 0)
        rm_material.Clear();

    shader_program_cache.Clear();
    variant_program_cache.Clear();

    ValueArray<ShaderModule *> shader_modules;
    shader_module_cache.GetValues(shader_modules);
    for (int i = 0; i < shader_modules.GetCount(); ++i)
    {
        delete shader_modules[i];
    }
    shader_module_cache.Clear();

    // 共享全局 pipeline layout 由本模块惰建拥有，材质（及以其创建的管线）
    // 清理完毕后在此销毁；否则会泄漏至 vkDestroyDevice（VUID-05137）。
    // VkPipelineLayout 与 VkPipeline 同为 VkDevice 的 child，先于管线销毁合法。
    if (shared_pipeline_layout_ != VK_NULL_HANDLE)
    {
        if (VulkanDevice *device = GetDevice())
            vkDestroyPipelineLayout(device->GetDevice(), shared_pipeline_layout_, nullptr);

        shared_pipeline_layout_ = VK_NULL_HANDLE;
    }
}

const ShaderModule *ShaderProgramManager::CreateShaderModule(const AnsiString &sm_name,const mtl::ShaderCreateInfo *sci)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);

    ShaderModule *sm = shader_module_cache.FindName(sci->GetShaderStage(), sm_name);
    if(sm)
        return sm;

    sm=device->CreateShaderModule((VkShaderStageFlagBits)sci->GetShaderStage(),sci->GetSPVData(),sci->GetSPVSize());

    if(!sm)
        return(nullptr);

    shader_module_cache.AddName(sci->GetShaderStage(), sm_name, sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                AnsiString shader_name = "Shader:" + sm_name + AnsiString(":") + GetShaderStageName((VkShaderStageFlagBits)sci->GetShaderStage());
                du->SetShaderModule(*sm, shader_name);
            }
        }
    #endif//_DEBUG

    return sm;
}

const ShaderModule *ShaderProgramManager::CreateShaderModule(const mtl::ShaderStageKey &key,
                                                             const mtl::ShaderCreateInfo *sci)
{
    if (!sci || key.stage != sci->GetShaderStage())
        return nullptr;

    return CreateShaderModule(key.ToString(), sci);
}

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const AnsiString &sm_name,
                                                                const VkShaderStageFlagBits stage,
                                                                const uint32_t *spv_data,
                                                                const size_t spv_size)
{
    VulkanDevice *device = GetDevice();
    if(!device)return(nullptr);
    if(sm_name.IsEmpty())return(nullptr);
    if(!spv_data||spv_size==0)return(nullptr);

    ShaderModule *sm = shader_module_cache.FindName(static_cast<ShaderStage>(stage), sm_name);
    if(sm)
        return sm;

    sm=device->CreateShaderModule(stage,spv_data,spv_size);

    if(!sm)
        return(nullptr);

    shader_module_cache.AddName(static_cast<ShaderStage>(stage), sm_name, sm);

    #ifdef _DEBUG
        {
            DebugUtils *du=device->GetDebugUtils();

            if(du)
            {
                AnsiString shader_name = "Shader:" + sm_name + AnsiString(":") + GetShaderStageName(stage);
                du->SetShaderModule(*sm, shader_name);
            }
        }
    #endif//_DEBUG

    return sm;
}

const ShaderModule *ShaderProgramManager::CreateShaderModuleFromSPV(const mtl::ShaderStageKey &key,
                                                                     const uint32_t *spv_data,
                                                                     const size_t spv_size)
{
    return CreateShaderModuleFromSPV(key.ToString(),
                                     static_cast<VkShaderStageFlagBits>(key.stage),
                                     spv_data,
                                     spv_size);
}

// ── compute：两条来源（现编 GLSL / 查表预烘焙 SPV）共用一个管线尾段 ─────────────

const ShaderModule *ShaderProgramManager::CreateComputeShaderModule(const AnsiString &shader_module_name,
                                                                    const AnsiString &glsl_source,
                                                                    const uint32_t push_constant_size)
{
    if(shader_module_name.IsEmpty()||glsl_source.IsEmpty())
        return(nullptr);

    if(ShaderModule *cached=shader_module_cache.FindName(ShaderStage::Compute,shader_module_name))
        return(cached);

#if ULRE_ENABLE_SHADERGEN
    mtl::ShaderCreateInfo sci(ShaderStage::Compute);

    sci.SetFinalGLSL(glsl_source.c_str());

    if(!sci.CompileFinalGLSLToSPV())
        return(nullptr);

    // 顺手烘焙：开发形态每跑一次就把 compute 产物写进 shader-cache，
    // 发行形态才有 compute_manifest 可查。只读 store（ULRE_SHADER_CACHE_MODE=readonly）
    // 下 Record 内部直接拒绝，不会污染既有缓存。
    if(mtl::ShaderArtifactStore *store=GetRuntimeShaderArtifactStore())
        mtl::RecordShaderComputeProgramArtifact(*store,
                                                shader_module_name.c_str(),
                                                sci.GetSPVData(),
                                                static_cast<uint64>(sci.GetSPVSize()),
                                                push_constant_size);

    return(CreateShaderModule(shader_module_name,&sci));
#else
    // 发行形态没有 GLSL→SPV 编译器。走到这里说明调用方还在用源码接口，
    // 应当改调 CreateComputePipelineFromCache。
    GLogError(u8"[ShaderProgramManager] 生成器已编译出构建（ULRE_ENABLE_SHADERGEN=0），"
                u8"无法在运行时编译 compute GLSL：%s",shader_module_name.c_str());
    return(nullptr);
#endif
}

ComputePipeline *ShaderProgramManager::BuildComputePipeline(const AnsiString &name,
                                                            const ShaderModule *sm,
                                                            const uint32_t push_constant_size)
{
    VulkanDevice *device=GetDevice();
    if(!device||!sm)return(nullptr);

    // 全局 Bindless 集必须已由 GraphicsContext::Init 就绪（Scene 集已退场）
    if(bindless_layout_==VK_NULL_HANDLE)
    {
        GLogError(u8"[ShaderProgramManager] global sets not ready, cannot create compute pipeline (init GraphicsContext first?)");
        return(nullptr);
    }

    // 128B 是 Vulkan spec 保证的 maxPushConstantsSize 下限；超限时不同设备行为不一，直接拒绝
    if(push_constant_size>128)
    {
        GLogError(u8"[ShaderProgramManager] push_constant_size %u exceeds 128B spec guarantee: %s",
                  push_constant_size,name.c_str());
        return(nullptr);
    }

    // BDA 终态：compute 管线与全材质布局同构——只剩 Bindless(0) 一个集合（Scene 集已退场），
    // 且带 DESCRIPTOR_BUFFER 位（spec 要求同一 pipeline layout 内的 set layout「全带或全不带」，
    // VUID-VkPipelineLayoutCreateInfo-pSetLayouts-08008）。用户数据一律走 BDA
    //（push constant 下发设备地址 + buffer_reference），无 set 无 binding。
    VkDescriptorSetLayout dsl[DESCRIPTOR_SET_TYPE_COUNT]={bindless_layout_};

    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType        =VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount=DESCRIPTOR_SET_TYPE_COUNT;
    layout_ci.pSetLayouts  =dsl;

    VkPushConstantRange pc_range{};

    if(push_constant_size>0)
    {
        pc_range.stageFlags =VK_SHADER_STAGE_COMPUTE_BIT;
        pc_range.offset     =0;
        pc_range.size       =push_constant_size;

        layout_ci.pushConstantRangeCount=1;
        layout_ci.pPushConstantRanges   =&pc_range;
    }

    VkPipelineLayout layout=VK_NULL_HANDLE;

    if(vkCreatePipelineLayout(device->GetDevice(),&layout_ci,nullptr,&layout)!=VK_SUCCESS)
    {
        GLogError(u8"[ShaderProgramManager] create compute pipeline layout failed: %s",name.c_str());
        return(nullptr);
    }

    #ifdef _DEBUG
        if(DebugUtils *du=device->GetDebugUtils())
            du->SetPipelineLayout(layout,AnsiString("PipelineLayout:Compute:")+name);
    #endif//_DEBUG

    // owns_pipeline_layout=true：专用 layout 随 ComputePipeline 析构销毁
    ComputePipeline *cp=device->CreateComputePipeline(name,*sm,layout,true);

    if(!cp)
        vkDestroyPipelineLayout(device->GetDevice(),layout,nullptr);

    return(cp);
}

ComputePipeline *ShaderProgramManager::CreateComputePipeline(const AnsiString &name,
                                                             const AnsiString &glsl_source,
                                                             const uint32_t push_constant_size)
{
    return BuildComputePipeline(name,
                                CreateComputeShaderModule(name,glsl_source,push_constant_size),
                                push_constant_size);
}

ComputePipeline *ShaderProgramManager::CreateComputePipelineFromCache(const AnsiString &name,
                                                                      const uint32_t push_constant_size)
{
    VulkanDevice *device=GetDevice();
    if(!device)return(nullptr);

    if(name.IsEmpty())
        return(nullptr);

    if(ShaderModule *cached=shader_module_cache.FindName(ShaderStage::Compute,name))
        return BuildComputePipeline(name,cached,push_constant_size);

    mtl::ShaderArtifactStore *store=GetRuntimeShaderArtifactStore();
    if(!store)
    {
        GLogError(u8"[ShaderProgramManager] no shader artifact store, cannot load compute program: %s",
                  name.c_str());
        return(nullptr);
    }

    ValueArray<uint8> spv;
    uint32_t baked_push_constant_size=0;

    if(!mtl::LoadShaderComputeProgramArtifact(*store,name.c_str(),spv,&baked_push_constant_size))
        return(nullptr);

    // 布局契约校验放在建管线之前：push constant 大小不一致意味着 SPV 里的偏移
    // 与本进程下发的偏移脱节，建出来就是越界读。
    if(baked_push_constant_size!=push_constant_size)
    {
        GLogError(u8"[ShaderProgramManager] compute program %s push constant size mismatch: baked=%u requested=%u",
                  name.c_str(),baked_push_constant_size,push_constant_size);
        return(nullptr);
    }

    const ShaderModule *sm=CreateShaderModuleFromSPV(name,
                                                    VK_SHADER_STAGE_COMPUTE_BIT,
                                                    reinterpret_cast<const uint32_t *>(spv.GetData()),
                                                    static_cast<size_t>(spv.GetCount()));
    if(!sm)
        return(nullptr);

    return BuildComputePipeline(name,sm,push_constant_size);
}

ShaderProgram *ShaderProgramManager::TryGetCachedShaderProgram(
    const mtl::ShaderProgramKey &key)
{
    return shader_program_cache.Find(key);
}

ShaderProgram *ShaderProgramManager::TryGetCachedByVariantKey(
    const uint64 variant_key) const
{
    mtl::ShaderProgramKey program_key{};
    if (!variant_program_cache.Get(variant_key, program_key))
        return nullptr;

    // 变体键命中后再走一次 program link 查表：program 被 Destroy 时只从
    // shader_program_cache 摘除，这里自然 miss，无需维护反向索引。
    return shader_program_cache.Find(program_key);
}

void ShaderProgramManager::RecordVariantProgramKey(
    const uint64 variant_key, const mtl::ShaderProgramKey &program_key)
{
    if (variant_key == 0)
        return;

    // 同一变体键恒对应同一 program link（发行形态查表依赖的就是这条不变量），
    // 因此已存在时覆盖与否等价；这里仍覆盖，避免将来键语义变动后留下旧值。
    if (!variant_program_cache.Add(variant_key, program_key))
    {
        mtl::ShaderProgramKey *slot =
            variant_program_cache.GetValuePointer(variant_key);
        if (slot)
            *slot = program_key;
    }
}

// 把编译好的 ShaderBuildContext 装配成可用的 ShaderProgram。
//
// 命名口径：**不叫 ...BuildPipeline** —— 本函数不创建 VkPipeline。VkPipeline 由渲染侧
// 按 pass 创建（RenderPass::CreatePipeline(ShaderProgram*, const MaterialRecipe&)，
// 见 RenderPrimitiveCollectSystem 的取用点）。这里只做：ShaderModule 装配 → stage 列表
// → 全材质共享 pipeline layout。
bool ShaderProgramManager::BuildRuntimeShaderProgram(ShaderProgram *mtl,
                                                     const AnsiString &mtl_name,
                                                     const mtl::ShaderBuildContext *ctx,
                                                     const mtl::ShaderCreateInfoMap &sci_map)
{
    if(!mtl || !ctx)
        return false;

    if(!BuildShaderModulesFromCreateInfoMap(this,
                                            mtl_name,
                                            sci_map,
                                            ctx,
                                            mtl->shader_maps,
                                            mtl))
    {
        return false;
    }

    CreateShaderStageList(mtl->shader_stage_list,mtl->shader_maps);

    // 片元 stage 保留判据**不在这里**（曾经有两层程序级扫描，2026-09-26 D8 实测后
    // 删除）：SPIRV 扫描常量写错从未命中、且唯一调用点位于 stage 缓存命中分支
    // （冷缓存首编译不扫）；这里的 FinalGLSL 文本扫描也看不见 include 进来的
    // alpha_compositor（实测 0 命中）。depth-only 通道的 FS 保留统一由 **recipe
    // 语义**（alpha_test/dither，与 ShadowCasterMasked 模板分派同源）判定，见
    // RenderPass::CreatePipeline(ShaderProgram*, const MaterialRecipe&)。
    //
    // mesh 化后顶点输入统一走 SSBO，无 VBO 顶点输入布局（VS 遗留 vertex_input 已删）

    // BDA 终态：全材质共享同一 pipeline layout（Scene(0)/Bindless(1) 全局集 + pc_root）——
    // 单例惰建缓存（GetOrCreateGlobalPipelineLayout），材质只持裸句柄、不拥有。
    mtl->pipeline_layout = GetOrCreateGlobalPipelineLayout();
    if(mtl->pipeline_layout == VK_NULL_HANDLE)
        return false;

    return true;
}

VkPipelineLayout ShaderProgramManager::GetOrCreateGlobalPipelineLayout()
{
    if(shared_pipeline_layout_ != VK_NULL_HANDLE)
        return shared_pipeline_layout_;

    VulkanDevice *device = GetDevice();
    if(!device)
        return VK_NULL_HANDLE;

    shared_pipeline_layout_ = device->CreateGlobalPipelineLayout(bindless_layout_);

    #ifdef _DEBUG
        if(shared_pipeline_layout_)
        {
            DebugUtils *du = device->GetDebugUtils();
            if(du)
                du->SetPipelineLayout(shared_pipeline_layout_, "PipelineLayout:Global");
        }
    #endif//_DEBUG

    return shared_pipeline_layout_;
}

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(
    const mtl::ShaderProgramKey &program_key,
    const mtl::ShaderBuildContext *ctx)
{
    HGL_CAPTURE_SCOPE();

    if (!ctx
     || !ctx->HasProgramLink()
     || !(ctx->GetProgramLink().BuildKey() == program_key))
        return(nullptr);

    // B9：program 级缓存先查——此前只 Add 不查（TryGetCachedShaderProgram
    // 无调用者，缓存从不命中，同 key 每次全新构建）。命中返回共享实例
    // （调用方不释放——manager 拥有；同 key = 同构建输入——P1-7 后
    // build_context_hash 含 purpose/profile/vertex_format，材质差异走
    // batch 级 descriptor override/bindless 索引）。
    if (auto *cached = TryGetCachedShaderProgram(program_key))
        return cached;

    const AnsiString mtl_name = program_key.ToString();
    ShaderProgramCreatePrecheckResult precheck_result;
    const ShaderProgramCreatePrecheckDecision precheck_decision = RunShaderProgramCreatePrecheck(
        ctx,
        mtl_name,
        precheck_result);

    if(precheck_decision != ShaderProgramCreatePrecheckDecision::Proceed)
    {
        GLogError("[ShaderProgramManager] shader program precheck rejected: name=%s decision=%u",
                  mtl_name.c_str(),
                  static_cast<uint32>(precheck_decision));
        return nullptr;
    }

    const mtl::ShaderCreateInfoMap &sci_map = *precheck_result.shader_map;

    AutoDelete<ShaderProgram> mtl=new ShaderProgram(mtl_name,ctx);
    if(!BuildRuntimeShaderProgram(mtl,
                                  mtl_name,
                                  ctx,
                                  sci_map))
        return nullptr;

    Add(mtl);

    shader_program_cache.Add(program_key, mtl);
    // ShaderProgram is a C++ object managed by ShaderProgramManager, not a Vulkan object
    // No need to track with ObjectTracker
    return mtl.Finish();
}

namespace
{
    // AcquireShaderProgram(request) 的公共前半段：
    // 解析 definition + 补齐 render template request。
    //
    // 纯算键早退与全量构建**必须**吃同一份解析结果，否则两条路径会在
    // render template roots 上分叉，算出不同的 program key。
    bool ResolveBuildRequest(
        const mtl::MaterialDefinitionBuildRequest &request,
        mtl::MaterialDefinition &out_definition,
        mtl::MaterialDefinitionBuildRequest &out_resolved_request)
    {
        if (!ResolveMaterialDefinitionForRequest(request, out_definition))
            return false;

        out_resolved_request = request;
        return ResolveRuntimeRenderTemplateRequest(
            out_definition, out_resolved_request,
            out_resolved_request.render_template_request);
    }

    /// 把「变体键 → program link」幂等写进烘焙清单。
    ///
    /// 只在产物确实已落进 store 时才写（`LoadProgramMetadata` 成功），避免清单
    /// 指向不存在的 SPV。`RecordShaderProgramManifestEntry` 内部 `Upsert` 幂等：
    /// 条目已存在且内容相同则直接成功返回，不重写磁盘。
    ///
    /// 落盘失败要报出来 —— 这是「烘焙产物不完整」的直接症状，静默会让问题漂到
    /// 发行期才以「少一批材质」的形式出现。
    void RecordBakedProgramManifestEntry(
        mtl::ShaderArtifactStore *artifact_store,
        const mtl::contract::PhysicalDeviceProfileLite *profile,
        const mtl::MaterialDefinition &definition,
        const mtl::MaterialDefinitionBuildRequest &resolved_request,
        const mtl::ShaderBuildContext *ctx)
    {
        if (!artifact_store || !ctx || !ctx->HasProgramLink())
            return;

        mtl::ShaderProgramArtifactMetadata baked_metadata{};
        if (!artifact_store->LoadProgramMetadata(ctx->GetProgramLink(), baked_metadata))
            return;

        mtl::ShaderProgramManifestEntry baked_entry{};
        baked_entry.lookup_key =
            mtl::ComputeMaterialProgramManifestKey(definition, resolved_request, profile);
        baked_entry.program_link = ctx->GetProgramLink();
        baked_entry.requires_recipe_runtime_rows = ctx->RequiresRecipeRuntimeRows();
        baked_entry.requires_recipe_payload_row = ctx->RequiresRecipePayloadRow();

        if (!mtl::RecordShaderProgramManifestEntry(artifact_store, profile, baked_entry))
        {
            GLogWarning("[ShaderProgramManager] failed to record shader program manifest entry: "
                        "id=%s key=0x%llx",
                        definition.definition_id.c_str(),
                        static_cast<unsigned long long>(baked_entry.lookup_key));
        }
    }
}//namespace

ShaderProgram *ShaderProgramManager::AcquireShaderProgram(
    const mtl::MaterialDefinitionBuildRequest &request)
{
    // Ensure recipe defaults are filled in before lookup, in case the caller skipped normalization.
    // This call is idempotent; primitive render paths will have already normalized.
    mtl::MaterialRecipe normalized_recipe = request.recipe;
    mtl::NormalizeRecipe(normalized_recipe);

    mtl::MaterialDefinitionBuildRequest normalized_request = request;
    normalized_request.recipe = normalized_recipe;
    normalized_request.defer_finalize = true;
    // 接通跨进程 SPV 磁盘缓存：FinalizeShaderBuildContext 先查缓存，未命中
    // 才走 glslang 编译并回填（BuildIfMissing）。FinalizeShaderBuildContext 在
    // store 非空时要求 program link + artifact metadata 齐备——生产路径的
    // BuildGenericMaterial 两者恒备（回归门已全变体验证）。
    normalized_request.shader_artifact_store = GetRuntimeShaderArtifactStore();

    mtl::MaterialDefinition definition{};
    mtl::MaterialDefinitionBuildRequest resolved_request{};
    if (!ResolveBuildRequest(normalized_request, definition, resolved_request))
        return nullptr;

    const mtl::contract::PhysicalDeviceProfileLite *profile =
        GetPhysicalDeviceProfile();

    // ── L1 变体键命中 ──
    //
    // 变体键是纯内存哈希（材质定义 + 已解析构建请求 + 设备 profile），零磁盘
    // 读、零 GLSL 发射。命中即直接返回，热路径上不再进入生成器。
    // 键为 0 表示算不出（极端退化），一律当 miss 落到下面全量路径。
    const uint64 variant_key = mtl::ComputeMaterialProgramManifestKey(
        definition, resolved_request, profile);

    if (ShaderProgram *cached = TryGetCachedByVariantKey(variant_key))
        return cached;

    mtl::ShaderProgramBuildBackend *build_backend = mtl::GetShaderProgramBuildBackend();
    if (!build_backend
     && mtl::EnsureShaderProgramManifestBackend(normalized_request.shader_artifact_store))
        build_backend = mtl::GetShaderProgramBuildBackend();

    if (!build_backend)
    {
        // 发行形态且尚未装 Manifest 后端时走到这里：明确报错，不能静默降级，
        // 否则会让「换个设备就少一批材质」变成难以定位的现象。
        GLogError("[ShaderProgramManager] No shader program build backend installed; "
                  "cannot build program for material id=%s name=%s",
                  definition.definition_id.c_str(),
                  definition.definition_name.c_str());
        return nullptr;
    }

    AutoDelete<mtl::ShaderBuildContext> ctx =
        build_backend->CreateBuildContext(profile, definition, resolved_request);
    if (!ctx)
    {
        GLogError("[ShaderProgramManager] Material definition build failed: id=%s name=%s",
                  definition.definition_id.c_str(),
                  definition.definition_name.c_str());
        return nullptr;
    }

    if (!ctx->HasProgramLink())
    {
        GLogError(
            "[ShaderProgramManager] Material build produced incomplete program identity: id=%s",
            definition.definition_id.c_str());
        return nullptr;
    }
    const mtl::ShaderProgramKey program_key =
        ctx->GetProgramLink().BuildKey();
    if (ShaderProgram *cached = TryGetCachedShaderProgram(program_key))
    {
        // 内容键（program link）命中时也必须回填清单：清单键是**变体**键，与
        // 内容键不是一一对应 —— 两个变体只要发射出的 GLSL 相同就共用一条
        // program link。此前只在「本次真的建出来」的分支写清单，于是这些
        // 共享变体在烘焙期永远拿不到 lookup_key 条目，发行期查表直接 miss
        //（Stage 1 缺陷 #1/#3）。
        // Upsert 幂等：条目已存在且内容相同时不会重写磁盘。
        RecordBakedProgramManifestEntry(
            normalized_request.shader_artifact_store, profile,
            definition, resolved_request, ctx);
        RecordVariantProgramKey(variant_key, program_key);
        return cached;
    }

    if (!build_backend->FinalizeBuildContext(ctx))
    {
        GLogError(
            "[ShaderProgramManager] Material build finalization failed: id=%s",
            definition.definition_id.c_str());
        return nullptr;
    }

    // ── 烘焙清单 ──
    //
    // 生成器形态在这里把「语义键 → program link」写进 program_manifest.pmf，发行
    // 形态（无生成器）就是靠它查表。只在产物确实已经落进 store 时才写，避免清单
    // 指向不存在的 SPV。Manifest 后端自己记录下来的是同一份数据，`Upsert` 幂等。
    RecordBakedProgramManifestEntry(
        normalized_request.shader_artifact_store, profile,
        definition, resolved_request, ctx);

    ShaderProgram *program = this->AcquireShaderProgram(program_key, ctx);
    if (program)
        RecordVariantProgramKey(variant_key, program_key);

    return program;
}

}//namespace hgl::graph
