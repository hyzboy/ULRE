#ifndef HGL_GRAPH_VULKAN_DATABASE_INCLUDE
#define HGL_GRAPH_VULKAN_DATABASE_INCLUDE

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/Mesh.h>
#include<hgl/type/ObjectManage.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

using PrimitiveID           =int;
using MeshID                =int;
using SamplerID             =int;
using StaticMeshID          =int;

using ShaderModuleMapByName=ObjectMap<AnsiString,ShaderModule>;

/**
 * 资源管理，用于管理场景内所需的所有数据
 */
class RenderResource
{
    VulkanDevice *device;
    
    IDObjectManage<PrimitiveID,            Primitive>          rm_primitives;              ///<图元合集
    IDObjectManage<SamplerID,              Sampler>            rm_samplers;                ///<采样器合集
    IDObjectManage<MeshID,                 Mesh>               rm_mesh;                    ///<渲染实例集合集

private:

public:

    VulkanDevice *GetDevice(){return device;}

public:

    RenderResource(VulkanDevice *dev):device(dev){}
    virtual ~RenderResource()=default;

public: //添加数据到管理器（如果指针为nullptr会返回-1）

    PrimitiveID             Add(Primitive *         p   ){return rm_primitives.Add(p);}
    SamplerID               Add(Sampler *           s   ){return rm_samplers.Add(s);}
    MeshID                  Add(Mesh *              r   ){return rm_mesh.Add(r);}

    Mesh *              CreateMesh(Primitive *r,MaterialInstance *mi,Pipeline *p);
    Mesh *              CreateMesh(PrimitiveCreater *pc,MaterialInstance *mi,Pipeline *p);

    Sampler *           CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *           CreateSampler(Texture *);

public: //Get

    Primitive *         GetPrimitive        (const PrimitiveID          &id){return rm_primitives.Get(id);}
    Sampler *           GetSampler          (const SamplerID            &id){return rm_samplers.Get(id);}
    Mesh *              GetMesh             (const MeshID               &id){return rm_mesh.Get(id);}

public: //Release

    void Release(Primitive *        p   ){rm_primitives.Release(p);}
    void Release(Sampler *          s   ){rm_samplers.Release(s);}
    void Release(Mesh *             r   ){rm_mesh.Release(r);}
};//class RenderResource
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
