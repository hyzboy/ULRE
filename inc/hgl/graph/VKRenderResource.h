#ifndef HGL_GRAPH_VULKAN_DATABASE_INCLUDE
#define HGL_GRAPH_VULKAN_DATABASE_INCLUDE

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/Mesh.h>
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
using StaticMeshID          =int;

using ShaderModuleMapByName=ObjectMap<AnsiString,ShaderModule>;
constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT=20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

/**
 * 资源管理，用于管理场景内所需的所有数据
 */
class RenderResource
{
    VulkanDevice *device;
    
    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];
    Map<AnsiString,Material *> material_by_name;
    
    IDObjectManage<MaterialID,             Material>           rm_material;                ///<材质合集
    IDObjectManage<MaterialInstanceID,     MaterialInstance>   rm_material_instance;       ///<材质实例合集
    IDObjectManage<DescriptorSetID,        DescriptorSet>      rm_desc_sets;               ///<描述符合集
    IDObjectManage<PrimitiveID,            Primitive>          rm_primitives;              ///<图元合集
    IDObjectManage<BufferID,               DeviceBuffer>       rm_buffers;                 ///<顶点缓冲区合集
    IDObjectManage<SamplerID,              Sampler>            rm_samplers;                ///<采样器合集
    IDObjectManage<RenderableID,           Mesh>         rm_renderables;             ///<渲染实例集合集

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

    VulkanDevice *GetDevice(){return device;}

    //注：并非一定要走这里，这里只是提供一个注册和自动绑定的机制
    DescriptorBinding static_descriptor;                                                    ///<静态属性描述符绑定管理
    DescriptorBinding global_descriptor;                                                    ///<全局属性描述符绑定管理

public:

    RenderResource(VulkanDevice *dev):device(dev),
        static_descriptor(DescriptorSetType::Static),
        global_descriptor(DescriptorSetType::Global)
    {}
    virtual ~RenderResource()=default;

public: //添加数据到管理器（如果指针为nullptr会返回-1）

    MaterialID              Add(Material *          mtl ){return rm_material.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}
    DescriptorSetID         Add(DescriptorSet *     ds  ){return rm_desc_sets.Add(ds);}
    PrimitiveID             Add(Primitive *         p   ){return rm_primitives.Add(p);}
    BufferID                Add(DeviceBuffer *      buf ){return rm_buffers.Add(buf);}
    SamplerID               Add(Sampler *           s   ){return rm_samplers.Add(s);}
    RenderableID            Add(Mesh *        r   ){return rm_renderables.Add(r);}

public: // VAB/VAO

    VAB *CreateVAB(VkFormat format,uint32_t count,const void *data, SharingMode sm=SharingMode::Exclusive);
    VAB *CreateVAB(VkFormat format,uint32_t count,                  SharingMode sm=SharingMode::Exclusive){return CreateVAB(format,             count,          nullptr,        sm);}

    #define SCENE_DB_CREATE_FUNC(name)  DeviceBuffer *Create##name(const AnsiString &buf_name,VkDeviceSize size,void *data,SharingMode sm=SharingMode::Exclusive);   \
                                        DeviceBuffer *Create##name(const AnsiString &buf_name,VkDeviceSize size,SharingMode sm=SharingMode::Exclusive){return Create##name(buf_name,size,nullptr,sm);}

            SCENE_DB_CREATE_FUNC(UBO)
            SCENE_DB_CREATE_FUNC(SSBO)
            SCENE_DB_CREATE_FUNC(INBO)

    #undef SCENE_DB_CREATE_FUNC

    IndexBuffer *CreateIBO(IndexType index_type,uint32_t count,const void *  data,  SharingMode sm=SharingMode::Exclusive);
    IndexBuffer *CreateIBO8 (                   uint32_t count,const uint8  *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U8 ,count,(void *)data,sm);}
    IndexBuffer *CreateIBO16(                   uint32_t count,const uint16 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16,count,(void *)data,sm);}
    IndexBuffer *CreateIBO32(                   uint32_t count,const uint32 *data,  SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32,count,(void *)data,sm);}

    IndexBuffer *CreateIBO(IndexType index_type,uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(index_type,      count,nullptr,sm);}
    IndexBuffer *CreateIBO8 (                   uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U8,   count,nullptr,sm);}
    IndexBuffer *CreateIBO16(                   uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U16,  count,nullptr,sm);}
    IndexBuffer *CreateIBO32(                   uint32_t count,SharingMode sm=SharingMode::Exclusive){return CreateIBO(IndexType::U32,  count,nullptr,sm);}

public: //Material

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name,const ShaderCreateInfo *);
    
    Material *          CreateMaterial(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *);
    Material *          LoadMaterial(const AnsiString &,mtl::Material2DCreateConfig *);
    Material *          LoadMaterial(const AnsiString &,mtl::Material3DCreateConfig *);

    MaterialInstance *  CreateMaterialInstance(Material *,const VILConfig *vil_cfg=nullptr);

    MaterialInstance *  CreateMaterialInstance(Material *,const VILConfig *vil_cfg,const void *,const int);

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const T *data)
    {
        return CreateMaterialInstance(mtl,vil_cfg,data,sizeof(T));
    }

    MaterialInstance *  CreateMaterialInstance(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *,const VILConfig *vil_cfg=nullptr);

    Mesh *        CreateMesh(Primitive *r,MaterialInstance *mi,Pipeline *p);
    Mesh *        CreateMesh(PrimitiveCreater *pc,MaterialInstance *mi,Pipeline *p);

    Sampler *           CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *           CreateSampler(Texture *);

public: //Get

    Material *          GetMaterial             (const MaterialID           &id){return rm_material.Get(id);}
    MaterialInstance *  GetMaterialInstance     (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}
    DescriptorSet *     GetDescSets             (const DescriptorSetID      &id){return rm_desc_sets.Get(id);}
    Primitive *         GetPrimitive            (const PrimitiveID          &id){return rm_primitives.Get(id);}
    DeviceBuffer *      GetBuffer               (const BufferID             &id){return rm_buffers.Get(id);}
    Sampler *           GetSampler              (const SamplerID            &id){return rm_samplers.Get(id);}
    Mesh *        GetRenderable           (const RenderableID         &id){return rm_renderables.Get(id);}

public: //Release

    void Release(Material *          mtl ){rm_material.Release(mtl);}
    void Release(MaterialInstance *  mi  ){rm_material_instance.Release(mi);}
    void Release(DescriptorSet *     ds  ){rm_desc_sets.Release(ds);}
    void Release(Primitive *         p   ){rm_primitives.Release(p);}
    void Release(DeviceBuffer *      buf ){rm_buffers.Release(buf);}
    void Release(Sampler *           s   ){rm_samplers.Release(s);}
    void Release(Mesh *        r   ){rm_renderables.Release(r);}
};//class RenderResource

/**
* 创建一个渲染资源对像<br>
* 这个函数是临时的，以后会被更好的机制替代
*/
template<typename T,typename ...ARGS>
T *CreateRRObject(RenderResource *rr,ARGS...args)
{
    if(!rr)
        return(nullptr);

    T *obj=T::CreateNewObject(rr,args...);

    if(!obj)
        return(nullptr);

    rr->Add(obj);
    return obj;
}
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
