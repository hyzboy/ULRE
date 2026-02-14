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
#include <hgl/graph/render/RenderFramework.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
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

    Material *GraphicsModule::CreateMaterial(const mtl::MaterialCreateInfo *mci)
    {
        if(!material_manager||!mci)
            return nullptr;

        AnsiString name="auto_mtl_"_str+AnsiString::numberOf(++auto_material_id);
        return material_manager->CreateMaterial(name,mci);
    }

    Material *GraphicsModule::LoadMaterial(const AnsiString &name)
    {
        if(!material_manager)
            return nullptr;

        mtl::Material2DCreateConfig cfg;
        return material_manager->LoadMaterial(name,&cfg);
    }

    MaterialInstance *GraphicsModule::CreateMaterialInstance(Material *mat)
    {
        return material_manager?material_manager->CreateMaterialInstance(mat):nullptr;
    }

    VKBuffer *GraphicsModule::CreateVAB(const void *data,VkDeviceSize size)
    {
        if(!buffer_manager||size==0)
            return nullptr;

        auto *vab=buffer_manager->CreateVAB(VK_FORMAT_R8_UINT,static_cast<uint32_t>(size),data);
        return reinterpret_cast<VKBuffer *>(vab);
    }

    DeviceBuffer *GraphicsModule::CreateUBO(VkDeviceSize size)
    {
        if(!buffer_manager)
            return nullptr;

        AnsiString name="auto_ubo_"_str+AnsiString::numberOf(++auto_buffer_id);
        return buffer_manager->CreateUBO(name,size);
    }

    DeviceBuffer *GraphicsModule::CreateSSBO(VkDeviceSize size)
    {
        if(!buffer_manager)
            return nullptr;

        AnsiString name="auto_ssbo_"_str+AnsiString::numberOf(++auto_buffer_id);
        return buffer_manager->CreateSSBO(name,size);
    }

    DeviceBuffer *GraphicsModule::CreateINBO(VkDeviceSize size)
    {
        if(!buffer_manager)
            return nullptr;

        AnsiString name="auto_inbo_"_str+AnsiString::numberOf(++auto_buffer_id);
        return buffer_manager->CreateINBO(name,size);
    }

    IndexBuffer *GraphicsModule::CreateIBO(const void *indices,uint32_t count)
    {
        return buffer_manager?buffer_manager->CreateIBO(IndexType::U16,count,indices):nullptr;
    }

    IndexBuffer *GraphicsModule::CreateIBO8(const void *indices,uint32_t count)
    {
        return buffer_manager?buffer_manager->CreateIBO8(count,static_cast<const uint8 *>(indices)):nullptr;
    }

    IndexBuffer *GraphicsModule::CreateIBO16(const void *indices,uint32_t count)
    {
        return buffer_manager?buffer_manager->CreateIBO16(count,static_cast<const uint16 *>(indices)):nullptr;
    }

    IndexBuffer *GraphicsModule::CreateIBO32(const void *indices,uint32_t count)
    {
        return buffer_manager?buffer_manager->CreateIBO32(count,static_cast<const uint32 *>(indices)):nullptr;
    }

    VertexDataManager *GraphicsModule::CreateVDM(const VIL *vil,VkDeviceSize vertices,VkDeviceSize indices,IndexType type)
    {
        if(!vil||vertices<=0||indices<=0||!device||!device->IsSupport(type))
            return nullptr;

        auto *vdm=new VertexDataManager(device,vil);
        if(!vdm)
            return nullptr;

        if(!vdm->Init(vertices,indices,type))
        {
            delete vdm;
            return nullptr;
        }

        return vdm;
    }

    SharedPtr<GeometryCreater> GraphicsModule::GetGeometryCreater(Material *mat)
    {
        if(!mat||!device)
            return nullptr;

        return new GeometryCreater(device,mat->GetDefaultVIL());
    }

    SharedPtr<GeometryCreater> GraphicsModule::GetGeometryCreater(MaterialInstance *mi)
    {
        if(!mi||!device)
            return nullptr;

        return new GeometryCreater(device,mi->GetVIL());
    }

    Geometry *GraphicsModule::CreateGeometry(const AnsiString &name,uint32_t vertex_count,const VIL *vil,
                                             const std::initializer_list<VertexAttribDataPtr> &vad_list)
    {
        if(!geometry_manager||!device||!vil)
            return nullptr;

        auto *pc=new GeometryCreater(device,vil);
        pc->Init(name,vertex_count);

        for(const auto &vad:vad_list)
        {
            if(!pc->WriteVAB(vad.name,vad.format,vad.data))
            {
                delete pc;
                return nullptr;
            }
        }

        auto *geometry=pc->Create();
        if(geometry)
            geometry_manager->Add(geometry);

        return geometry;
    }

    Primitive *GraphicsModule::CreatePrimitive(const AnsiString &name,uint32_t vertex_count,MaterialInstance *mi,
                                               Pipeline *p,const std::initializer_list<VertexAttribDataPtr> &vad_list)
    {
        if(!primitive_manager||!mi)
            return nullptr;

        auto *geometry=CreateGeometry(name,vertex_count,mi->GetVIL(),vad_list);
        if(!geometry)
            return nullptr;

        return primitive_manager->CreatePrimitive(geometry,mi,p);
    }

    Primitive *GraphicsModule::CreatePrimitive(Geometry *geo,MaterialInstance *mi,Pipeline *p)
    {
        return primitive_manager?primitive_manager->CreatePrimitive(geo,mi,p):nullptr;
    }

    Primitive *GraphicsModule::CreatePrimitive(GeometryCreater *pc,MaterialInstance *mi,Pipeline *p)
    {
        return primitive_manager?primitive_manager->CreatePrimitive(pc,mi,p):nullptr;
    }

    Pipeline *GraphicsModule::CreatePipeline(Material *mat,const InlinePipeline &ip)
    {
        auto *rp=GetDefaultRenderPass();
        return rp?rp->CreatePipeline(mat,ip):nullptr;
    }

    Pipeline *GraphicsModule::CreatePipeline(MaterialInstance *mi,const InlinePipeline &ip)
    {
        auto *rp=GetDefaultRenderPass();
        return rp?rp->CreatePipeline(mi,ip):nullptr;
    }

    RenderPass *GraphicsModule::GetDefaultRenderPass()
    {
        if(default_render_pass)
            return default_render_pass;

        return legacy_rf?legacy_rf->GetDefaultRenderPass():nullptr;
    }

    Texture2D *GraphicsModule::LoadTexture2D(const OSString &filename,bool auto_mipmap)
    {
        return tex_manager?tex_manager->LoadTexture2D(filename,auto_mipmap):nullptr;
    }

    TextureCube *GraphicsModule::LoadTextureCube(const OSString &filename,bool auto_mipmap)
    {
        return tex_manager?tex_manager->LoadTextureCube(filename,auto_mipmap):nullptr;
    }

    Texture2DArray *GraphicsModule::CreateTexture2DArray(const AnsiString &name,uint32_t width,uint32_t height,
                                                         uint32_t layer,VkFormat fmt,bool auto_mipmap)
    {
        return tex_manager?tex_manager->CreateTexture2DArray(name,width,height,layer,fmt,auto_mipmap):nullptr;
    }

    bool GraphicsModule::LoadTexture2DArray(Texture2DArray *tex,uint32_t layer,const OSString &filename)
    {
        return tex_manager?tex_manager->LoadTexture2DArray(tex,layer,filename):false;
    }

    Sampler *GraphicsModule::CreateSampler(VkSamplerCreateInfo *sci)
    {
        return sampler_manager?sampler_manager->CreateSampler(sci):nullptr;
    }

    Sampler *GraphicsModule::CreateSampler(Texture *tex)
    {
        return sampler_manager?sampler_manager->CreateSampler(tex):nullptr;
    }

    TextRender *GraphicsModule::CreateTextRender(FontSource *fs,int limit)
    {
        auto *rp=GetDefaultRenderPass();
        if(!rp)
            return nullptr;

        return TextRender::CreateWithGraphicsContext(this,rp,fs,limit,nullptr);
    }

    void GraphicsModule::Add(Geometry *geometry)
    {
        if(geometry_manager&&geometry)
            geometry_manager->Add(geometry);
    }
}
