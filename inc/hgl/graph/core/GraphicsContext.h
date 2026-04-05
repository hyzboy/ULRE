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
#include <hgl/graph/module/ShaderGenValidationTypes.h>

namespace hgl::graph
{
    namespace mtl::contract
    {
        struct PhysicalDeviceProfileLite;
    }

    // Forward declarations
    class GraphModuleManager;
    class TextureManager;
    class RenderTargetManager;
    class MaterialManager;
    class BufferManager;
    class SamplerManager;
    class GeometryManager;
    class PrimitiveManager;
    class MaterialAssetRegistry;

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

        TextureManager *tex_manager = nullptr;
        RenderTargetManager *rt_manager = nullptr;
        MaterialManager *material_manager = nullptr;
        BufferManager *buffer_manager = nullptr;
        SamplerManager *sampler_manager = nullptr;
        GeometryManager *geometry_manager = nullptr;
        PrimitiveManager *primitive_manager = nullptr;
        MaterialAssetRegistry *material_asset_registry = nullptr;

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
        // Device访问
        VulkanDevice *GetDevice() const { return device; }
        VulkanDevAttr *GetDevAttr() const;
        VulkanPhyDevice *GetPhyDevice() const;
        const mtl::contract::PhysicalDeviceProfileLite *GetPhysicalDeviceProfile() const;
        VkDevice GetVkDevice() const;

        // 模块管理器访问
        TextureManager *GetTextureManager() { return tex_manager; }
        MaterialManager *GetMaterialManager() { return material_manager; }
        BufferManager *GetBufferManager() { return buffer_manager; }
        SamplerManager *GetSamplerManager() { return sampler_manager; }
        GeometryManager *GetGeometryManager() { return geometry_manager; }
        PrimitiveManager *GetPrimitiveManager() { return primitive_manager; }
        MaterialAssetRegistry *GetMaterialAssetRegistry() { return material_asset_registry; }

        // 扩展访问（不常用）
        GraphModuleManager *GetModuleManager() { return module_manager; }
        RenderTargetManager *GetRenderTargetManager() { return rt_manager; }

        // ShaderGen Profiler debug entry (collect-only, no default output)
        void ResetShaderGenProfiler();
        ShaderGenProfilerSnapshot GetShaderGenProfilerSnapshot() const;

        // ShaderGen Validation report query entry (collect-only, no default output)
        bool GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name = nullptr) const;
        std::vector<ShaderGenValidationReportRecord> GetShaderGenRecentValidationReports(uint32_t max_count = 64) const;
        std::map<std::string, uint32_t> GetShaderGenRecentValidationCategoryHistogram(uint32_t max_count = 128) const;
    };

    // 向后兼容别名
    using IGraphicsContext = GraphicsContext;

} // namespace hgl::graph
