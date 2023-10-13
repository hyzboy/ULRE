#ifndef HGL_GRAPH_VULKAN_DATABASE_INCLUDE
#define HGL_GRAPH_VULKAN_DATABASE_INCLUDE

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VertexAttribData.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/type/ObjectManage.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/VKDescriptorBindingManage.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

namespace mtl
{
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
}//namespace mtl

using MaterialID            =int;
using MaterialInstanceID    =int;
using BufferID              =int;
using DescriptorSetID       =int;
using PrimitiveID           =int;
using RenderableID          =int;
using SamplerID             =int;
using TextureID             =int;

class VertexAttribData;

using ShaderModuleMapByName=ObjectMap<AnsiString,ShaderModule>;
constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT=20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

/**
 * 资源管理，用于管理场景内所需的所有数据
 */
class RenderResource
{
    GPUDevice *device;
    
    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];
    Map<AnsiString,Material *> material_by_name;
    Map<OSString,Texture *> texture_by_name;
    
    IDObjectManage<MaterialID,             Material>           rm_material;                ///<材质合集
    IDObjectManage<MaterialInstanceID,     MaterialInstance>   rm_material_instance;       ///<材质实例合集
    IDObjectManage<DescriptorSetID,        DescriptorSet>      rm_desc_sets;               ///<描述符合集
    IDObjectManage<PrimitiveID,            Primitive>          rm_primitives;              ///<图元合集
    IDObjectManage<BufferID,               DeviceBuffer>       rm_buffers;                 ///<顶点缓冲区合集
    IDObjectManage<SamplerID,              Sampler>            rm_samplers;                ///<采样器合集
    IDObjectManage<TextureID,              Texture>            rm_textures;                ///<纹理合集
    IDObjectManage<RenderableID,           Renderable>         rm_renderables;             ///<渲染实例集合集

private:

    void AddBuffer(const AnsiString &buf_name,DeviceBuffer *buf)
    {
        rm_buffers.Add(buf);

    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            du->SetBuffer(buf->GetBuffer(),buf_name+":Buffer");
            du->SetDeviceMemory(buf->GetVkMemory(),buf_name+":Memory");
        }
    #endif//_DEBUG
    }

public:

    //注：并非一定要走这里，这里只是提供一个注册和自动绑定的机制
    DescriptorBinding global_descriptor;                                                   ///<全局属性描述符绑定管理

public:

    RenderResource(GPUDevice *dev):device(dev),global_descriptor(DescriptorSetType::Global){}
    virtual ~RenderResource()=default;

public: //Add

    MaterialID              Add(Material *          mtl ){return rm_material.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}
    DescriptorSetID         Add(DescriptorSet *     ds  ){return rm_desc_sets.Add(ds);}
    PrimitiveID             Add(Primitive *         p   ){return rm_primitives.Add(p);}
    BufferID                Add(DeviceBuffer *      buf ){return rm_buffers.Add(buf);}
    SamplerID               Add(Sampler *           s   ){return rm_samplers.Add(s);}
    TextureID               Add(Texture *           t   ){return rm_textures.Add(t);}
    RenderableID            Add(Renderable *        r   ){return rm_renderables.Add(r);}

public: // VBO/VAO

    VBO *CreateVBO(VkFormat format,uint32_t count,const void *data,SharingMode sm=SharingMode::Exclusive);
    VBO *CreateVBO(VkFormat format,uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateVBO(format,count,nullptr,sm);}
    VBO *CreateVBO(const VAD *vad,SharingMode sm=SharingMode::Exclusive){return CreateVBO(vad->GetFormat(),vad->GetCount(),vad->GetData(),sm);}

    #define SCENE_DB_CREATE_FUNC(name)  DeviceBuffer *Create##name(const AnsiString &buf_name,VkDeviceSize size,void *data,SharingMode sm=SharingMode::Exclusive);   \
                                        DeviceBuffer *Create##name(const AnsiString &buf_name,VkDeviceSize size,SharingMode sm=SharingMode::Exclusive){return Create##name(buf_name,size,nullptr,sm);}

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

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name,const ShaderCreateInfo *);
    
    Material *          CreateMaterial(const mtl::MaterialCreateInfo *);
    Material *          LoadMaterial(const AnsiString &,mtl::Material2DCreateConfig *);
    Material *          LoadMaterial(const AnsiString &,mtl::Material3DCreateConfig *);

    MaterialInstance *  CreateMaterialInstance(Material *,const VILConfig *vil_cfg=nullptr);

    MaterialInstance *  CreateMaterialInstance(Material *,const VILConfig *vil_cfg,const void *,const int);

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const T *data)
    {
        return CreateMaterialInstance(mtl,vil_cfg,*data,sizeof(T));
    }

    MaterialInstance *  CreateMaterialInstance(const mtl::MaterialCreateInfo *,const VILConfig *vil_cfg=nullptr);

    Primitive *         CreatePrimitive(const AnsiString &,const uint32_t vertex_count=0);

    Renderable *        CreateRenderable(Primitive *r,MaterialInstance *mi,Pipeline *p);

    Sampler *           CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *           CreateSampler(Texture *);

public: //texture

    Texture2D *         LoadTexture2D(const OSString &,bool auto_mipmaps=false);
    TextureCube *       LoadTextureCube(const OSString &,bool auto_mipmaps=false);

    Texture2DArray *    CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps=false);
    bool                LoadTexture2DToArray(Texture2DArray *,const uint32_t layer,const OSString &);

public: //Get

    Material *          GetMaterial             (const MaterialID           &id){return rm_material.Get(id);}
    MaterialInstance *  GetMaterialInstance     (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}
    DescriptorSet *     GetDescSets             (const DescriptorSetID      &id){return rm_desc_sets.Get(id);}
    Primitive *         GetPrimitive            (const PrimitiveID          &id){return rm_primitives.Get(id);}
    DeviceBuffer *      GetBuffer               (const BufferID             &id){return rm_buffers.Get(id);}
    Sampler *           GetSampler              (const SamplerID            &id){return rm_samplers.Get(id);}
    Texture *           GetTexture              (const TextureID            &id){return rm_textures.Get(id);}
    Renderable *        GetRenderable           (const RenderableID         &id){return rm_renderables.Get(id);}
};//class RenderResource
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
