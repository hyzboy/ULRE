#pragma once

/**
 * GraphicsContext - 图形系统资源管理
 *
 * 用来替代旧的集中式渲染入口过度集中化设计
 *
 * 职责：
 * - 聚合所有图形资源管理器（Manager）
 * - 提供统一的资源访问入口
 * - 管理模块生命周期
 *
 * 使用方式：
 * ```cpp
 * auto* graphics = app->GetGraphicsContext();
 * auto* material = graphics->GetMaterialManager()->Create(...);
 * auto* buffer = graphics->GetBufferManager()->CreateUBO(...);
 * ```
 */

#include <hgl/vk/VKDevice.h>

namespace hgl::graph
{
    namespace mtl::contract
    {
        struct PhysicalDeviceProfileLite;
    }

    // Forward declarations
    class GraphModuleManager;
    class RenderPassManager;
    class TextureManager;
    class RenderTargetManager;
    class ShaderProgramManager;
    class BufferManager;
    class SamplerManager;
    class GeometryManager;
    class ResourceDomainManager;
    class EnvironmentManager;
    class BindlessTextureManager;
    class GlobalSceneUBOSet;

    /**
     * GraphicsContext - Vulkan图形资源管理器聚合类
     *
     * 聚合所有图形资源Manager，提供统一访问入口。
     * 解耦应用框架与图形资源管理。
     *
     * 所有权：
     * - 拥有所有管理器（通过GraphModuleManager）
     * - 引用VulkanDevice（不拥有）
     *
     * 生命周期：
     * - 由AppFramework在初始化时创建
     * - 传递给ECSContext和其他系统使用
     * - 在VulkanDevice销毁前销毁
     */
    class GraphicsContext
    {
    private:
        VulkanDevice *device = nullptr;

        GraphModuleManager *module_manager = nullptr;

        RenderPassManager *rp_manager = nullptr;
        TextureManager *tex_manager = nullptr;
        RenderTargetManager *rt_manager = nullptr;
        ShaderProgramManager *material_manager = nullptr;
        BufferManager *buffer_manager = nullptr;
        SamplerManager *sampler_manager = nullptr;
        GeometryManager *geometry_manager = nullptr;
        ResourceDomainManager *resource_domain_manager = nullptr;
        EnvironmentManager *env_manager = nullptr;
        BindlessTextureManager *bindless_texture_manager_ = nullptr;
        GlobalSceneUBOSet *global_scene_ubo_set_ = nullptr;

    public:
        explicit GraphicsContext(VulkanDevice *dev);
        ~GraphicsContext();

        // Disable copy
        GraphicsContext(const GraphicsContext &) = delete;
        GraphicsContext &operator=(const GraphicsContext &) = delete;

    public:
        /**
         * 初始化所有图形模块
         * 必须在构造后、使用前调用
         * @return true 成功, false 失败
         */
        bool Initialize();

        /**
         * 关闭所有模块
         * 在销毁前调用以确保正确的清理顺序
         */
        void Shutdown();

        /**
         * 通知所有模块窗口尺寸改变
         * @param extent 新的窗口尺寸
         */
        void OnResize(const VkExtent2D &extent);

    public:
        template<typename T> T *GetManager();
        template<typename T> T *Get() { return GetManager<T>(); }

        // Device访问
        VulkanDevice *GetDevice() const { return device; }
        VulkanDevAttr *GetDevAttr() const;
        VulkanPhyDevice *GetPhyDevice() const;
        const mtl::contract::PhysicalDeviceProfileLite *GetPhysicalDeviceProfile() const;
        VkDevice GetVkDevice() const;

        // 模块管理器访问
        RenderPassManager *GetRenderPassManager() { return rp_manager; }
        TextureManager *GetTextureManager() { return tex_manager; }
        ShaderProgramManager *GetMaterialManager() { return material_manager; }
        BufferManager *GetBufferManager() { return buffer_manager; }
        SamplerManager *GetSamplerManager() { return sampler_manager; }
        GeometryManager *GetGeometryManager() { return geometry_manager; }
        ResourceDomainManager *GetResourceDomainManager() { return resource_domain_manager; }
        EnvironmentManager *GetEnvironmentManager() { return env_manager; }
        BindlessTextureManager *GetBindlessTextureManager() { return bindless_texture_manager_; }
        const BindlessTextureManager *GetBindlessTextureManager() const { return bindless_texture_manager_; }

        GlobalSceneUBOSet *GetGlobalSceneUBOSet() { return global_scene_ubo_set_; }
        const GlobalSceneUBOSet *GetGlobalSceneUBOSet() const { return global_scene_ubo_set_; }

        // 扩展访问（不常用）
        GraphModuleManager *GetModuleManager() { return module_manager; }
        RenderTargetManager *GetRenderTargetManager() { return rt_manager; }

    };

    // 向后兼容别名
    using IGraphicsContext = GraphicsContext;

    template<typename T>
    inline T *GraphicsContext::GetManager()
    {
        return nullptr;
    }

    template<>
    inline RenderPassManager *GraphicsContext::GetManager<RenderPassManager>()
    {
        return GetRenderPassManager();
    }

    template<>
    inline TextureManager *GraphicsContext::GetManager<TextureManager>()
    {
        return GetTextureManager();
    }

    template<>
    inline RenderTargetManager *GraphicsContext::GetManager<RenderTargetManager>()
    {
        return GetRenderTargetManager();
    }

    template<>
    inline ShaderProgramManager *GraphicsContext::GetManager<ShaderProgramManager>()
    {
        return GetMaterialManager();
    }

    template<>
    inline BufferManager *GraphicsContext::GetManager<BufferManager>()
    {
        return GetBufferManager();
    }

    template<>
    inline SamplerManager *GraphicsContext::GetManager<SamplerManager>()
    {
        return GetSamplerManager();
    }

    template<>
    inline GeometryManager *GraphicsContext::GetManager<GeometryManager>()
    {
        return GetGeometryManager();
    }

    template<>
    inline ResourceDomainManager *GraphicsContext::GetManager<ResourceDomainManager>()
    {
        return GetResourceDomainManager();
    }

    template<>
    inline GraphModuleManager *GraphicsContext::GetManager<GraphModuleManager>()
    {
        return GetModuleManager();
    }

    template<>
    inline EnvironmentManager *GraphicsContext::GetManager<EnvironmentManager>()
    {
        return GetEnvironmentManager();
    }

    template<>
    inline BindlessTextureManager *GraphicsContext::GetManager<BindlessTextureManager>()
    {
        return GetBindlessTextureManager();
    }

} // namespace hgl::graph
