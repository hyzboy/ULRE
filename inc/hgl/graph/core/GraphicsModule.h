#pragma once

#include <cstdint>
#include <hgl/graph/core/GraphicsContext.h>
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
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/type/Smart.h>
#include <hgl/type/String.h>
#include <hgl/vk/pipeline/VKInlinePipeline.h>
#include <hgl/vk/pipeline/VKPipeline.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/vk/VK.h>
#include <hgl/vk/VKBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKDeviceAttribute.h>
#include <hgl/vk/VKIndexBuffer.h>
#include <hgl/vk/VKMaterial.h>
#include <hgl/vk/VKMaterialInstance.h>
#include <hgl/vk/VKPhysicalDevice.h>
#include <hgl/vk/VKRenderPass.h>
#include <hgl/vk/VKSampler.h>
#include <hgl/vk/VKTexture.h>
#include <initializer_list>
#include <vulkan/vulkan_core.h>

namespace hgl::graph
{

    class GraphicsModule final: public IGraphicsContext
    {
    public:
        GraphicsModule(VulkanDevice *device,
                       RenderPassManager *rp_manager,
                       TextureManager *tex_manager,
                       MaterialManager *material_manager,
                       BufferManager *buffer_manager,
                       SamplerManager *sampler_manager,
                       GeometryManager *geometry_manager,
                       PrimitiveManager *primitive_manager);

        VulkanDevice *GetDevice() const override { return device; }
        VulkanDevAttr *GetDevAttr() const override;
        VulkanPhyDevice *GetPhyDevice() const override;
        VkDevice              GetVkDevice() const override;

        RenderPassManager *GetRenderPassManager() override { return rp_manager; }
        TextureManager *GetTextureManager() override { return tex_manager; }
        MaterialManager *GetMaterialManager() override { return material_manager; }
        BufferManager *GetBufferManager() override { return buffer_manager; }
        SamplerManager *GetSamplerManager() override { return sampler_manager; }
        GeometryManager *GetGeometryManager() override { return geometry_manager; }
        PrimitiveManager *GetPrimitiveManager() override { return primitive_manager; }

    private:

        VulkanDevice *device=nullptr;
        RenderPassManager *rp_manager=nullptr;
        TextureManager *tex_manager=nullptr;
        MaterialManager *material_manager=nullptr;
        BufferManager *buffer_manager=nullptr;
        SamplerManager *sampler_manager=nullptr;
        GeometryManager *geometry_manager=nullptr;
        PrimitiveManager *primitive_manager=nullptr;
        uint64_t auto_material_id=0;
        uint64_t auto_buffer_id=0;
    };
} // namespace hgl::graph
