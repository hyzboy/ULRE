#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKPhysicalDevice.h>
#include <hgl/graph/module/GraphModuleManager.h>
#include <hgl/graph/module/RenderPassManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/module/RenderTargetManager.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>

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

        geometry_manager = module_manager->GetOrCreate<GeometryManager>();
        if (!geometry_manager)
            return false;

        primitive_manager = module_manager->GetOrCreate<PrimitiveManager>();
        if (!primitive_manager)
            return false;

        material_manager = module_manager->GetOrCreate<MaterialManager>();
        if (!material_manager)
            return false;

        buffer_manager = module_manager->GetOrCreate<BufferManager>();
        if (!buffer_manager)
            return false;

        // Set graphics context for module manager
        module_manager->SetGraphicsContext(this);

        return true;
    }

    void GraphicsContext::Shutdown()
    {
        if (device)
            device->WaitIdle();

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
        primitive_manager = nullptr;
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
        if (primitive_manager) primitive_manager->OnResize(extent);
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

    void GraphicsContext::ResetShaderGenProfiler()
    {
        if (material_manager)
            material_manager->ResetShaderGenProfiler();
    }

    ShaderGenProfilerSnapshot GraphicsContext::GetShaderGenProfilerSnapshot() const
    {
        if (material_manager)
            return material_manager->GetShaderGenProfilerSnapshot();

        return {};
    }

    bool GraphicsContext::GetShaderGenLastValidationReport(ShaderGenValidationReport &out_report, std::string *out_material_name) const
    {
        if (material_manager)
            return material_manager->GetShaderGenLastValidationReport(out_report, out_material_name);

        out_report = {};
        if (out_material_name)
            out_material_name->clear();
        return false;
    }

    std::vector<ShaderGenValidationReportRecord> GraphicsContext::GetShaderGenRecentValidationReports(uint32_t max_count) const
    {
        if (material_manager)
            return material_manager->GetShaderGenRecentValidationReports(max_count);

        (void)max_count;
        return {};
    }

    std::map<std::string, uint32_t> GraphicsContext::GetShaderGenRecentValidationCategoryHistogram(uint32_t max_count) const
    {
        if (material_manager)
            return material_manager->GetShaderGenRecentValidationCategoryHistogram(max_count);

        (void)max_count;
        return {};
    }

} // namespace hgl::graph
