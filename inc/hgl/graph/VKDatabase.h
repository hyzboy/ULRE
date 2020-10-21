#ifndef HGL_GRAPH_VULKAN_DATABASE_INCLUDE
#define HGL_GRAPH_VULKAN_DATABASE_INCLUDE

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSets.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttribData.h>
#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/font/TextRenderable.h>
#include<hgl/type/ResManage.h>
VK_NAMESPACE_BEGIN
using MaterialID            =int;
using MaterialInstanceID    =int;
using PipelineID            =int;
using BufferID              =int;
using DescriptorSetsID      =int;
using RenderableID          =int;
using RenderableInstanceID  =int;
using SamplerID             =int;
using TextureID             =int;

class VertexAttribData;

/**
 * 资源管理，用于管理场景内所需的所有数据
 */
class Database
{
    Device *device;
    
    MapObject<OSString,ShaderModule> shader_module_by_name;
    Map<OSString,Material *> material_by_name;
    
    IDResManage<MaterialID,             Material>           rm_material;                ///<材质合集
    IDResManage<MaterialInstanceID,     MaterialInstance>   rm_material_instance;       ///<材质实例合集
    IDResManage<PipelineID,             Pipeline>           rm_pipeline;                ///<管线合集
    IDResManage<DescriptorSetsID,       DescriptorSets>     rm_desc_sets;               ///<描述符合集
    IDResManage<RenderableID,           Renderable>         rm_renderables;             ///<可渲染对象合集
    IDResManage<BufferID,               Buffer>             rm_buffers;                 ///<顶点缓冲区合集
    IDResManage<SamplerID,              Sampler>            rm_samplers;                ///<采样器合集
    IDResManage<TextureID,              Texture>            rm_textures;                ///<纹理合集
    IDResManage<RenderableInstanceID,   RenderableInstance> rm_renderable_instances;    ///<渲染实例集合集

public:

    Database(Device *dev):device(dev){}
    virtual ~Database()=default;

public: //Add

    MaterialID              Add(Material *          mtl ){return rm_material.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}
    PipelineID              Add(Pipeline *          p   ){return rm_pipeline.Add(p);}
    DescriptorSetsID        Add(DescriptorSets *    ds  ){return rm_desc_sets.Add(ds);}
    RenderableID            Add(Renderable *        r   ){return rm_renderables.Add(r);}
    BufferID                Add(Buffer *            buf ){return rm_buffers.Add(buf);}
    SamplerID               Add(Sampler *           s   ){return rm_samplers.Add(s);}
    TextureID               Add(Texture *           t   ){return rm_textures.Add(t);}
    RenderableInstanceID    Add(RenderableInstance *ri  ){return rm_renderable_instances.Add(ri);}

public: // VBO/VAO

    VAB *CreateVAB(VkFormat format,uint32_t count,const void *data,SharingMode sm=SharingMode::Exclusive);
    VAB *CreateVAB(VkFormat format,uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateVAB(format,count,nullptr,sm);}
    VAB *CreateVAB(const VAD *vad,SharingMode sm=SharingMode::Exclusive){return CreateVAB(vad->GetVulkanFormat(),vad->GetCount(),vad->GetData(),sm);}

    #define SCENE_DB_CREATE_FUNC(name)  Buffer *Create##name(VkDeviceSize size,void *data,SharingMode sm=SharingMode::Exclusive);   \
                                        Buffer *Create##name(VkDeviceSize size,SharingMode sm=SharingMode::Exclusive);

            SCENE_DB_CREATE_FUNC(UBO)
            SCENE_DB_CREATE_FUNC(SSBO)
            SCENE_DB_CREATE_FUNC(INBO)

    #undef SCENE_DB_CREATE_FUNC

    IndexBuffer *CreateIBO(IndexType index_type,uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive);
    IndexBuffer *CreateIBO16(                   uint32_t count,const uint16 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16,count,(void *)data,sm);}
    IndexBuffer *CreateIBO32(                   uint32_t count,const uint32 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32,count,(void *)data,sm);}

    IndexBuffer *CreateIBO(IndexType index_type,uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(index_type,count,nullptr,sm);}
    IndexBuffer *CreateIBO16(                   uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16,count,nullptr,sm);}
    IndexBuffer *CreateIBO32(                   uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32,count,nullptr,sm);}

public: //Material

    const ShaderModule *CreateShaderModule(const OSString &filename,ShaderResource *shader_resource);
    const ShaderModule *CreateShaderModule(const OSString &filename);
    
    Material *          CreateMaterial(const OSString &vertex_shader_filename,const OSString &fragment_shader_filename);    
    Material *          CreateMaterial(const OSString &vertex_shader_filename,const OSString &geometry_shader_filename,const OSString &fragment_shader_filename);

    Material *          CreateMaterial(const OSString &);
    MaterialInstance *  CreateMaterialInstance(Material *);
    MaterialInstance *  CreateMaterialInstance(const OSString &);
    
    Pipeline *          CreatePipeline(Material *,          RenderTarget *,const InlinePipeline &,  const Prim &prim=Prim::Triangles,const bool prim_restart=false);
    Pipeline *          CreatePipeline(MaterialInstance *,  RenderTarget *,const InlinePipeline &,  const Prim &prim=Prim::Triangles,const bool prim_restart=false);
    Pipeline *          CreatePipeline(Material *,          RenderTarget *,      PipelineData *,    const Prim &prim=Prim::Triangles,const bool prim_restart=false);
    Pipeline *          CreatePipeline(MaterialInstance *,  RenderTarget *,      PipelineData *,    const Prim &prim=Prim::Triangles,const bool prim_restart=false);
    Pipeline *          CreatePipeline(Material *,          RenderTarget *,const OSString &,        const Prim &prim=Prim::Triangles,const bool prim_restart=false);
    Pipeline *          CreatePipeline(MaterialInstance *,  RenderTarget *,const OSString &,        const Prim &prim=Prim::Triangles,const bool prim_restart=false);

    Renderable *        CreateRenderable(const uint32_t vertex_count=0);
    TextRenderable *    CreateTextRenderable(Material *);

    RenderableInstance *CreateRenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p);

    Sampler *           CreateSampler(VkSamplerCreateInfo *sci=nullptr);

public: //Get

    Material *          GetMaterial             (const MaterialID           &id){return rm_material.Get(id);}
    MaterialInstance *  GetMaterialInstance     (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}
    Pipeline *          GetPipeline             (const PipelineID           &id){return rm_pipeline.Get(id);}
    DescriptorSets *    GetDescSets             (const DescriptorSetsID     &id){return rm_desc_sets.Get(id);}
    Renderable *        GetRenderable           (const RenderableID         &id){return rm_renderables.Get(id);}
    Buffer *            GetBuffer               (const BufferID             &id){return rm_buffers.Get(id);}
    Sampler *           GetSampler              (const SamplerID            &id){return rm_samplers.Get(id);}
    Texture *           GetTexture              (const TextureID            &id){return rm_textures.Get(id);}
    RenderableInstance *GetRenderableInstance   (const RenderableInstanceID &id){return rm_renderable_instances.Get(id);}
};//class Database
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
