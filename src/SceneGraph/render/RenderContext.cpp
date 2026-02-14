#include <hgl/graph/render/RenderContext.h>
#include <hgl/graph/mtl/Material2DCreateConfig.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/mtl/MaterialLibrary.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/type/Smart.h>
#include <hgl/Charset.h>
#include <hgl/utf.h>
#include <hgl/vk/VertexDataManager.h>
#include <hgl/vk/VKVertexInput.h>

namespace hgl::graph
{
    RenderContext::RenderContext(VulkanDevice* dev,
                                 TextureManager* tex_mgr,
                                 BufferManager* buf_mgr,
                                 MaterialManager* mat_mgr,
                                 SamplerManager* samp_mgr,
                                 RenderPassManager* rp_mgr,
                                 GeometryManager* geo_mgr,
                                 PrimitiveManager* prim_mgr)
        : device(dev)
        , texture_manager(tex_mgr)
        , buffer_manager(buf_mgr)
        , material_manager(mat_mgr)
        , sampler_manager(samp_mgr)
        , render_pass_manager(rp_mgr)
        , geometry_manager(geo_mgr)
        , primitive_manager(prim_mgr)
    {
    }

    Material* RenderContext::CreateMaterial(const AnsiString& name)
    {
        if (!material_manager || !device)
            return nullptr;

        mtl::Material2DCreateConfig cfg2d;
        AutoDelete<mtl::MaterialCreateInfo> mci = mtl::CreateMaterialCreateInfo(device->GetDevAttr(), name, &cfg2d);

        if (!mci)
        {
            mtl::Material3DCreateConfig cfg3d;
            mci = mtl::CreateMaterialCreateInfo(device->GetDevAttr(), name, &cfg3d);
        }

        if (!mci)
            return nullptr;

        return material_manager->CreateMaterial(name, mci);
    }

    Material* RenderContext::CreateMaterial(const AnsiString& name, const mtl::MaterialCreateInfo* mci)
    {
        return material_manager ? material_manager->CreateMaterial(name, mci) : nullptr;
    }

    Material* RenderContext::LoadMaterial(const OSString& path)
    {
        if (!material_manager)
            return nullptr;

        const AnsiString mtl_name = ToAnsiString(UTF8CharSet, path);

        mtl::Material2DCreateConfig cfg2d;
        Material* mtl = material_manager->LoadMaterial(mtl_name, &cfg2d);
        if (mtl)
            return mtl;

        mtl::Material3DCreateConfig cfg3d;
        return material_manager->LoadMaterial(mtl_name, &cfg3d);
    }

    Material* RenderContext::LoadMaterial(const AnsiString& name, mtl::Material2DCreateConfig* cfg)
    {
        return material_manager ? material_manager->LoadMaterial(name, cfg) : nullptr;
    }

    Material* RenderContext::LoadMaterial(const AnsiString& name, mtl::Material3DCreateConfig* cfg)
    {
        return material_manager ? material_manager->LoadMaterial(name, cfg) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(Material* material)
    {
        if (!material_manager)
            return nullptr;

        return material_manager->CreateMaterialInstance(material);
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(Material* material, const VIL* vil)
    {
        return material_manager ? material_manager->CreateMaterialInstance(material, vil) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(Material* material, const VILConfig* vil_cfg)
    {
        return material_manager ? material_manager->CreateMaterialInstance(material, vil_cfg) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(Material* material, const VIL* vil, const void* data, const uint32 data_size)
    {
        return material_manager ? material_manager->CreateMaterialInstance(material, vil, data, data_size) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(Material* material, const VILConfig* vil_cfg, const void* data, const uint32 data_size)
    {
        return material_manager ? material_manager->CreateMaterialInstance(material, vil_cfg, data, data_size) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(const AnsiString& name, const mtl::MaterialCreateInfo* mci, const VILConfig* vil_cfg)
    {
        return material_manager ? material_manager->CreateMaterialInstance(name, mci, vil_cfg) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(const AnsiString& name, const mtl::MaterialCreateInfo* mci, const VILConfig* vil_cfg, const void* data, const uint32 data_size)
    {
        return material_manager ? material_manager->CreateMaterialInstance(name, mci, vil_cfg, data, data_size) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(const AnsiString& name, mtl::Material2DCreateConfig* cfg, const VILConfig* vil_cfg, const void* data, const uint32 data_size)
    {
        return material_manager ? material_manager->CreateMaterialInstance(name, cfg, vil_cfg, data, data_size) : nullptr;
    }

    MaterialInstance* RenderContext::CreateMaterialInstance(const AnsiString& name, mtl::Material3DCreateConfig* cfg, const VILConfig* vil_cfg, const void* data, const uint32 data_size)
    {
        return material_manager ? material_manager->CreateMaterialInstance(name, cfg, vil_cfg, data, data_size) : nullptr;
    }

    DeviceBuffer* RenderContext::CreateUBO(const AnsiString& name, VkDeviceSize size)
    {
        return buffer_manager ? buffer_manager->CreateUBO(name, size, nullptr) : nullptr;
    }

    DeviceBuffer* RenderContext::CreateSSBO(const AnsiString& name, VkDeviceSize size)
    {
        return buffer_manager ? buffer_manager->CreateSSBO(name, size, nullptr) : nullptr;
    }

    DeviceBuffer* RenderContext::CreateINBO(const AnsiString& name, VkDeviceSize size)
    {
        return buffer_manager ? buffer_manager->CreateINBO(name, size, nullptr) : nullptr;
    }

    namespace
    {
        uint32_t IndexStride(IndexType type)
        {
            switch (type)
            {
                case IndexType::U8:  return 1;
                case IndexType::U16: return 2;
                case IndexType::U32: return 4;
                default:             return 0;
            }
        }
    }

    IndexBuffer* RenderContext::CreateIBO(VkDeviceSize size, IndexType type)
    {
        if (!buffer_manager)
            return nullptr;

        const uint32_t stride = IndexStride(type);
        if (stride == 0)
            return nullptr;

        const uint32_t count = static_cast<uint32_t>(size / stride);
        if (count == 0)
            return nullptr;

        return buffer_manager->CreateIBO(type, count, nullptr);
    }

    VAB* RenderContext::CreateVAB(VkFormat format, uint32_t count, const void* data)
    {
        return buffer_manager ? buffer_manager->CreateVAB(format, count, data) : nullptr;
    }

    Texture2D* RenderContext::LoadTexture2D(const OSString& path, bool auto_mipmap)
    {
        return texture_manager ? texture_manager->LoadTexture2D(path, auto_mipmap) : nullptr;
    }

    TextureCube* RenderContext::LoadTextureCube(const OSString& base_path, bool auto_mipmaps)
    {
        return texture_manager ? texture_manager->LoadTextureCube(base_path, auto_mipmaps) : nullptr;
    }

    Texture2DArray* RenderContext::CreateTexture2DArray(const AnsiString& name,
                                                        uint32_t width,
                                                        uint32_t height,
                                                        uint32_t layer,
                                                        VkFormat fmt,
                                                        bool auto_mipmaps)
    {
        return texture_manager ? texture_manager->CreateTexture2DArray(name, width, height, layer, fmt, auto_mipmaps) : nullptr;
    }

    bool RenderContext::LoadTexture2DArray(Texture2DArray* tex2d_array, uint32_t layer, const OSString& path)
    {
        return texture_manager ? texture_manager->LoadTexture2DArray(tex2d_array, layer, path) : false;
    }

    Sampler* RenderContext::CreateSampler(VkSamplerCreateInfo* create_info)
    {
        return sampler_manager ? sampler_manager->CreateSampler(create_info) : nullptr;
    }

    Sampler* RenderContext::CreateSampler(Texture* texture)
    {
        return sampler_manager ? sampler_manager->CreateSampler(texture) : nullptr;
    }

    Pipeline* RenderContext::CreatePipeline(Material* material,
                                            const VertexInputLayout* vil,
                                            const PipelineData* pd,
                                            bool prim_restart)
    {
        if (!current_render_target)
            return nullptr;

        RenderPass* rp = current_render_target->GetRenderPass();
        return rp ? rp->CreatePipeline(material, vil, pd, prim_restart) : nullptr;
    }

    VertexDataManager* RenderContext::CreateVDM(const VertexInputLayout* vil,
                                                VkDeviceSize vertices_count,
                                                VkDeviceSize indices_count,
                                                IndexType type)
    {
        if (!device || !vil || vertices_count == 0)
            return nullptr;

        if (indices_count == 0)
            indices_count = vertices_count;

        if (!device->IsSupport(type))
            return nullptr;

        auto* vdm = new VertexDataManager(device, vil);
        if (!vdm)
            return nullptr;

        if (!vdm->Init(vertices_count, indices_count, type))
        {
            delete vdm;
            return nullptr;
        }

        return vdm;
    }

    Geometry* RenderContext::CreateGeometry(const AnsiString& name,
                                            uint32_t vert_count,
                                            const VertexInputLayout* vil,
                                            const std::initializer_list<VertexAttribDataPtr>& vad_list)
    {
        if (!device || !geometry_manager)
            return nullptr;

        auto* pc = new GeometryCreater(device, vil);
        pc->Init(name, vert_count);

        for (const auto& vad : vad_list)
        {
            if (!pc->WriteVAB(vad.name, vad.format, vad.data))
            {
                delete pc;
                return nullptr;
            }
        }

        auto* geometry = pc->Create();
        if (geometry)
            geometry_manager->Add(geometry);

        return geometry;
    }

    Primitive* RenderContext::CreatePrimitive(const AnsiString& name,
                                              uint32_t vert_count,
                                              MaterialInstance* mi,
                                              Pipeline* pipeline,
                                              const std::initializer_list<VertexAttribDataPtr>& vad_list)
    {
        if (!primitive_manager)
            return nullptr;

        auto* geometry = CreateGeometry(name, vert_count, mi->GetVIL(), vad_list);
        if (!geometry)
            return nullptr;

        return primitive_manager->CreatePrimitive(geometry, mi, pipeline);
    }

    void RenderContext::SetCurrentRenderTarget(IRenderTarget* rt)
    {
        current_render_target = rt;
    }

    IRenderTarget* RenderContext::GetCurrentRenderTarget() const
    {
        return current_render_target;
    }

    void RenderContext::SetCurrentRenderCmdBuffer(RenderCmdBuffer* cmd)
    {
        current_render_cmd_buf = cmd;
    }

    RenderCmdBuffer* RenderContext::GetCurrentRenderCmdBuffer() const
    {
        return current_render_cmd_buf;
    }
} // namespace hgl::graph
