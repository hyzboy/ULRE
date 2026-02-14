#pragma once

#include <hgl/graph/core/GraphicsContext.h>
#include <cstdint>

namespace hgl::graph
{
    class RenderFramework;

    class GraphicsModule final : public IGraphicsContext
    {
    public:
        GraphicsModule(vk::VulkanDevice* device,
                       RenderPassManager* rp_manager,
                       TextureManager* tex_manager,
                       MaterialManager* material_manager,
                       BufferManager* buffer_manager,
                       SamplerManager* sampler_manager,
                       GeometryManager* geometry_manager,
                       PrimitiveManager* primitive_manager);

        void SetDefaultRenderPass(RenderPass* render_pass) { default_render_pass = render_pass; }
        void SetLegacyRenderFramework(RenderFramework* rf) { legacy_rf = rf; }

        vk::VulkanDevice*     GetDevice() const override { return device; }
        vk::VulkanDevAttr*    GetDevAttr() const override;
        vk::VulkanPhyDevice*  GetPhyDevice() const override;
        VkDevice              GetVkDevice() const override;

        RenderPassManager* GetRenderPassManager() override { return rp_manager; }
        TextureManager*    GetTextureManager() override { return tex_manager; }
        MaterialManager*   GetMaterialManager() override { return material_manager; }
        BufferManager*     GetBufferManager() override { return buffer_manager; }
        SamplerManager*    GetSamplerManager() override { return sampler_manager; }
        GeometryManager*   GetGeometryManager() override { return geometry_manager; }
        PrimitiveManager*  GetPrimitiveManager() override { return primitive_manager; }

        Material*          CreateMaterial(const mtl::MaterialCreateInfo* mci) override;
        Material*          LoadMaterial(const AnsiString& name) override;
        MaterialInstance*  CreateMaterialInstance(Material* mat) override;

        VKBuffer*          CreateVAB(const void* data = nullptr, VkDeviceSize size = 0) override;
        DeviceBuffer*      CreateUBO(VkDeviceSize size) override;
        DeviceBuffer*      CreateSSBO(VkDeviceSize size) override;
        DeviceBuffer*      CreateINBO(VkDeviceSize size) override;

        IndexBuffer*       CreateIBO(const void* indices, uint32_t count) override;
        IndexBuffer*       CreateIBO8(const void* indices, uint32_t count) override;
        IndexBuffer*       CreateIBO16(const void* indices, uint32_t count) override;
        IndexBuffer*       CreateIBO32(const void* indices, uint32_t count) override;

        VertexDataManager* CreateVDM(const VIL* vil, VkDeviceSize vertices, VkDeviceSize indices = 0, IndexType type = IndexType::U16) override;
        SharedPtr<GeometryCreater> GetGeometryCreater(Material* mat) override;
        SharedPtr<GeometryCreater> GetGeometryCreater(MaterialInstance* mi) override;

        Geometry*          CreateGeometry(const AnsiString& name, uint32_t vertex_count, const VIL* vil,
                                          const std::initializer_list<VertexAttribDataPtr>& vad_list) override;
        Primitive*         CreatePrimitive(const AnsiString& name, uint32_t vertex_count, MaterialInstance* mi,
                                           Pipeline* p, const std::initializer_list<VertexAttribDataPtr>& vad_list) override;
        Primitive*         CreatePrimitive(Geometry* geo, MaterialInstance* mi, Pipeline* p) override;
        Primitive*         CreatePrimitive(GeometryCreater* pc, MaterialInstance* mi, Pipeline* p) override;

        Pipeline*          CreatePipeline(Material* mat, const InlinePipeline& ip) override;
        RenderPass*        GetDefaultRenderPass() override;

        Texture2D*         LoadTexture2D(const OSString& filename, bool auto_mipmap = true) override;
        TextureCube*       LoadTextureCube(const OSString& filename, bool auto_mipmap = false) override;
        Texture2DArray*    CreateTexture2DArray(const AnsiString& name, uint32_t width, uint32_t height,
                                                uint32_t layer, VkFormat fmt, bool auto_mipmap = false) override;
        bool               LoadTexture2DArray(Texture2DArray* tex, uint32_t layer, const OSString& filename) override;

        Sampler*           CreateSampler(VkSamplerCreateInfo* sci = nullptr) override;
        Sampler*           CreateSampler(Texture* tex) override;

        TextRender*        CreateTextRender(FontSource* fs, int limit = 1024) override;

        void               Add(Geometry* geometry) override;

    private:
        vk::VulkanDevice* device = nullptr;
        RenderPassManager* rp_manager = nullptr;
        TextureManager* tex_manager = nullptr;
        MaterialManager* material_manager = nullptr;
        BufferManager* buffer_manager = nullptr;
        SamplerManager* sampler_manager = nullptr;
        GeometryManager* geometry_manager = nullptr;
        PrimitiveManager* primitive_manager = nullptr;
        RenderPass* default_render_pass = nullptr;
        RenderFramework* legacy_rf = nullptr;
        uint64_t auto_material_id = 0;
        uint64_t auto_buffer_id = 0;
    };
} // namespace hgl::graph
