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
    class RenderFramework;

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

        void SetDefaultRenderPass(RenderPass *render_pass) { default_render_pass=render_pass; }
        void SetLegacyRenderFramework(RenderFramework *rf) { legacy_rf=rf; }

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

        Material *CreateMaterial(const mtl::MaterialCreateInfo *mci) override;
        Material *CreateMaterial(const AnsiString &name) override;
        Material *CreateMaterial(const AnsiString &name,const mtl::MaterialCreateInfo *mci) override;
        Material *LoadMaterial(const OSString &path) override;
        Material *LoadMaterial(const AnsiString &name) override;
        Material *LoadMaterial(const AnsiString &name,mtl::Material2DCreateConfig *cfg) override;
        Material *LoadMaterial(const AnsiString &name,mtl::Material3DCreateConfig *cfg) override;
        MaterialInstance *CreateMaterialInstance(Material *mat) override;
        MaterialInstance *CreateMaterialInstance(Material *mat,const VIL *vil) override;
        MaterialInstance *CreateMaterialInstance(Material *mat,const VILConfig *vil_cfg) override;
        MaterialInstance *CreateMaterialInstance(Material *mat,const VIL *vil,const void *data,const uint32 data_size) override;
        MaterialInstance *CreateMaterialInstance(Material *mat,const VILConfig *vil_cfg,const void *data,const uint32 data_size) override;

        MaterialInstance *CreateMaterialInstance(const AnsiString &name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg) override;
        MaterialInstance *CreateMaterialInstance(const AnsiString &name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg,const void *data,const uint32 data_size) override;
        MaterialInstance *CreateMaterialInstance(const AnsiString &name,mtl::Material2DCreateConfig *cfg,const VILConfig *vil_cfg,const void *data,const uint32 data_size) override;
        MaterialInstance *CreateMaterialInstance(const AnsiString &name,mtl::Material3DCreateConfig *cfg,const VILConfig *vil_cfg,const void *data,const uint32 data_size) override;

        VKBuffer *CreateVAB(const void *data=nullptr,VkDeviceSize size=0) override;
        DeviceBuffer *CreateUBO(VkDeviceSize size) override;
        DeviceBuffer *CreateSSBO(VkDeviceSize size) override;
        DeviceBuffer *CreateINBO(VkDeviceSize size) override;

        DeviceBuffer *CreateUBO(const AnsiString &name,VkDeviceSize size) override;
        DeviceBuffer *CreateSSBO(const AnsiString &name,VkDeviceSize size) override;
        DeviceBuffer *CreateINBO(const AnsiString &name,VkDeviceSize size) override;

        IndexBuffer *CreateIBO(const void *indices,uint32_t count) override;
        IndexBuffer *CreateIBO8(const void *indices,uint32_t count) override;
        IndexBuffer *CreateIBO16(const void *indices,uint32_t count) override;
        IndexBuffer *CreateIBO32(const void *indices,uint32_t count) override;
        IndexBuffer *CreateIBO(VkDeviceSize size,IndexType type) override;

        VertexDataManager *CreateVDM(const VIL *vil,VkDeviceSize vertices,VkDeviceSize indices=0,IndexType type=IndexType::U16) override;
        SharedPtr<GeometryCreater> GetGeometryCreater(Material *mat) override;
        SharedPtr<GeometryCreater> GetGeometryCreater(MaterialInstance *mi) override;

        Geometry *CreateGeometry(const AnsiString &name,uint32_t vertex_count,const VIL *vil,
                     const std::initializer_list<VertexAttribDataPtr> &vad_list) override;
        Primitive *CreatePrimitive(const AnsiString &name,uint32_t vertex_count,MaterialInstance *mi,
                                   Pipeline *p,const std::initializer_list<VertexAttribDataPtr> &vad_list) override;
        Primitive *CreatePrimitive(Geometry *geo,MaterialInstance *mi,Pipeline *p) override;
        Primitive *CreatePrimitive(GeometryCreater *pc,MaterialInstance *mi,Pipeline *p) override;

        Pipeline *CreatePipeline(Material *mat,const InlinePipeline &ip) override;
        Pipeline *CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip) override;
        RenderPass *GetDefaultRenderPass() override;

        Texture2D *LoadTexture2D(const OSString &filename,bool auto_mipmap=true) override;
        TextureCube *LoadTextureCube(const OSString &filename,bool auto_mipmap=false) override;
        Texture2DArray *CreateTexture2DArray(const AnsiString &name,uint32_t width,uint32_t height,
                                             uint32_t layer,VkFormat fmt,bool auto_mipmap=false) override;
        bool               LoadTexture2DArray(Texture2DArray *tex,uint32_t layer,const OSString &filename) override;

        Sampler *CreateSampler(VkSamplerCreateInfo *sci=nullptr) override;
        Sampler *CreateSampler(Texture *tex) override;

        TextRender *CreateTextRender(FontSource *fs,int limit=1024) override;

        void               Add(Geometry *geometry) override;

    private:

        VulkanDevice *device=nullptr;
        RenderPassManager *rp_manager=nullptr;
        TextureManager *tex_manager=nullptr;
        MaterialManager *material_manager=nullptr;
        BufferManager *buffer_manager=nullptr;
        SamplerManager *sampler_manager=nullptr;
        GeometryManager *geometry_manager=nullptr;
        PrimitiveManager *primitive_manager=nullptr;
        RenderPass *default_render_pass=nullptr;
        RenderFramework *legacy_rf=nullptr;
        uint64_t auto_material_id=0;
        uint64_t auto_buffer_id=0;
    };
} // namespace hgl::graph
