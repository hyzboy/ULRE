#include <cstdint>
#include <hgl/CoreType.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/core/GraphicsModule.h>
#include <hgl/graph/font/FontSource.h>
#include <hgl/graph/font/TextRender.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/RenderPassManager.h>
#include <hgl/graph/module/SamplerManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/mtl/Material2DCreateConfig.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/mtl/MaterialLibrary.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/Charset.h>
#include <hgl/utf.h>
#include <hgl/type/Smart.h>
#include <hgl/type/String.h>
#include <hgl/vk/pipeline/VKInlinePipeline.h>
#include <hgl/vk/pipeline/VKPipeline.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/vk/VK.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKIndexBuffer.h>
#include <hgl/vk/VKMaterial.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKRenderPass.h>
#include <hgl/vk/VKSampler.h>
#include <hgl/vk/VKTexture.h>
#include <initializer_list>
#include <vulkan/vulkan_core.h>

namespace hgl::graph
{
    GraphicsModule::GraphicsModule(VulkanDevice *in_device,
                                   RenderPassManager *in_rp_manager,
                                   TextureManager *in_tex_manager,
                                   MaterialManager *in_material_manager,
                                   BufferManager *in_buffer_manager,
                                   SamplerManager *in_sampler_manager,
                                   GeometryManager *in_geometry_manager,
                                   PrimitiveManager *in_primitive_manager)
        : device(in_device)
        ,rp_manager(in_rp_manager)
        ,tex_manager(in_tex_manager)
        ,material_manager(in_material_manager)
        ,buffer_manager(in_buffer_manager)
        ,sampler_manager(in_sampler_manager)
        ,geometry_manager(in_geometry_manager)
        ,primitive_manager(in_primitive_manager)
    {}

    VulkanDevAttr *GraphicsModule::GetDevAttr() const
    {
        return device?device->GetDevAttr():nullptr;
    }

    VulkanPhyDevice *GraphicsModule::GetPhyDevice() const
    {
        return device?const_cast<VulkanPhyDevice*>(device->GetPhyDevice()):nullptr;
    }

    VkDevice GraphicsModule::GetVkDevice() const
    {
        return device?device->GetDevice():nullptr;
    }
}
