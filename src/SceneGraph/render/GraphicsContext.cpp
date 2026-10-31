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
#include <hgl/framework/LoadedFileManager.h>
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

    bool GraphicsContext::LoadMaterialRecipeFile(const OSString &path)
    {
        return material_recipe_registry_.LoadFile(path, tex_manager);
    }

    bool GraphicsContext::LoadMaterialRecipeDirectory(
        const OSString &path,
        int *out_file_count,
        int *out_error_count)
    {
        return material_recipe_registry_.LoadDirectory(
            path, tex_manager, out_file_count, out_error_count);
    }

    const mtl::MaterialRecipe *GraphicsContext::FindMaterialRecipe(
        const char *name) const
    {
        return material_recipe_registry_.FindByName(name);
    }

    const mtl::MaterialRecipe *GraphicsContext::FindOrLoadMaterialRecipe(
        const char *name)
    {
        if (!name || !name[0])
        {
            GLogError(u8"[GraphicsContext] 无法查找材质配方：名称为空");
            return nullptr;
        }

        if (const mtl::MaterialRecipe *recipe =
                material_recipe_registry_.FindByName(name))
            return recipe;

        if (!default_material_recipes_loaded_)
        {
            const OSString recipe_directory =
                (filesystem::Path(ToOSString(mtl::GetShaderLibraryPath()))
                 / OSString(OS_TEXT("asset/recipe"))).ToOSString();
            int loaded_count = 0;
            int error_count = 0;
            if (!LoadMaterialRecipeDirectory(recipe_directory, &loaded_count, &error_count))
            {
                GLogError(u8"[GraphicsContext] 加载默认材质配方目录失败: recipe=%s directory=%ls",
                          name, recipe_directory.c_str());
                return nullptr;
            }

            if (error_count)
                GLogWarning(u8"[GraphicsContext] 部分材质配方加载失败: directory=%ls loaded=%d errors=%d",
                            recipe_directory.c_str(), loaded_count, error_count);

            default_material_recipes_loaded_ = true;
        }

        const mtl::MaterialRecipe *recipe = material_recipe_registry_.FindByName(name);
        if (!recipe)
        {
            GLogError(u8"[GraphicsContext] 材质配方未找到: recipe=%s registered=%d",
                      name, material_recipe_registry_.GetCount());
        }
        else
        {
            GLogDebug(u8"[GraphicsContext] 材质配方已解析: recipe=%s definition=%s",
                      name, recipe->mtl_def_id.c_str());
        }
        return recipe;
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

        // 全局异步文件加载底座：后台单 IO 线程（同一物理设备串行读盘更稳），
        // 主线程侧由各消费系统在自己的更新阶段调用 Pump()/Collect()。
        // 必须在任何消费者（纹理流式加载系统等）创建之前就位。
        loaded_file_manager_ = new LoadedFileManager();

        if (!loaded_file_manager_ || !loaded_file_manager_->Start(1))
        {
            GLogError("[GraphicsContext] LoadedFileManager Start failed");
            return false;
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

        scene_asset_registry_.Clear();

        // 先收掉后台加载底座：Stop(Drain) 会等所有已提交读盘结束并同步派发取消回调，
        // 保证此后不再有 IO 线程访问本上下文持有的任何资源。必须早于模块销毁。
        if (loaded_file_manager_)
        {
            loaded_file_manager_->Stop();
            SAFE_CLEAR(loaded_file_manager_)
        }

        // 图元资产绑定表持有几何/配方注册器的裸指针 ⇒ 必须先于它们清空。
        if (primitive_asset_registry_ready_)
        {
            primitive_asset_registry_.Clear();
            primitive_asset_registry_.SetSources(nullptr, nullptr);
            primitive_asset_registry_ready_ = false;
        }

        // Recipe data contains TextureAsset observers owned by TextureManager.
        // Drop the cache before destroying the module manager that owns them.
        material_recipe_registry_.Clear();
        default_material_recipes_loaded_ = false;

        // 几何资产句柄拥有各自的 Geometry，几何析构会向 MeshDrawParams 池归还行号，
        // 因此必须在模块（含 GlobalSSBOBufferRegistry / GeometryManager）销毁之前清空。
        if (geometry_asset_registry_ready_)
        {
            geometry_asset_registry_.Clear();
            geometry_asset_registry_.SetTargets(nullptr, nullptr);
            geometry_asset_registry_ready_ = false;
        }

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
