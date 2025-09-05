#ifndef HGL_GRAPH_VULKAN_DATABASE_INCLUDE
#define HGL_GRAPH_VULKAN_DATABASE_INCLUDE

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/Mesh.h>
#include<hgl/type/ObjectManage.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

using PrimitiveID           =int;
using MeshID                =int;
using StaticMeshID          =int;

using ShaderModuleMapByName=ObjectMap<AnsiString,ShaderModule>;

/**
 * 资源管理，用于管理场景内所需的所有数据
 */
class RenderResource
{
    VulkanDevice *device;
    
    IDObjectManage<PrimitiveID,            Primitive>          rm_primitives;              ///<图元合集

private:

public:

    VulkanDevice *GetDevice(){return device;}

public:

    RenderResource(VulkanDevice *dev):device(dev){}
    virtual ~RenderResource()=default;

public: //添加数据到管理器（如果指针为nullptr会返回-1）

    PrimitiveID             Add(Primitive *         p   ){return rm_primitives.Add(p);}

public: //Get

    Primitive *         GetPrimitive        (const PrimitiveID          &id){return rm_primitives.Get(id);}

public: //Release

    void Release(Primitive *        p   ){rm_primitives.Release(p);}    
};//class RenderResource
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
