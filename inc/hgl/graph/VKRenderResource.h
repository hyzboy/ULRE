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

using BufferID              =int;

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
    IDObjectManage<BufferID,               DeviceBuffer>       rm_buffers;                 ///<顶点缓冲区合集
    IDObjectManage<SamplerID,              Sampler>            rm_samplers;                ///<采样器合集
    IDObjectManage<MeshID,                 Mesh>               rm_mesh;                    ///<渲染实例集合集

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

public:

    RenderResource(VulkanDevice *dev):device(dev){}
    virtual ~RenderResource()=default;

public: //添加数据到管理器（如果指针为nullptr会返回-1）

    PrimitiveID             Add(Primitive *         p   ){return rm_primitives.Add(p);}
    BufferID                Add(DeviceBuffer *      buf ){return rm_buffers.Add(buf);}
    SamplerID               Add(Sampler *           s   ){return rm_samplers.Add(s);}
    MeshID                  Add(Mesh *              r   ){return rm_mesh.Add(r);}

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

    Mesh *              CreateMesh(Primitive *r,MaterialInstance *mi,Pipeline *p);
    Mesh *              CreateMesh(PrimitiveCreater *pc,MaterialInstance *mi,Pipeline *p);

    Sampler *           CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *           CreateSampler(Texture *);

public: //Get

    Primitive *         GetPrimitive        (const PrimitiveID          &id){return rm_primitives.Get(id);}
    DeviceBuffer *      GetBuffer           (const BufferID             &id){return rm_buffers.Get(id);}
    Sampler *           GetSampler          (const SamplerID            &id){return rm_samplers.Get(id);}
    Mesh *              GetMesh             (const MeshID               &id){return rm_mesh.Get(id);}

public: //Release

    void Release(Primitive *        p   ){rm_primitives.Release(p);}
    void Release(DeviceBuffer *     buf ){rm_buffers.Release(buf);}
    void Release(Sampler *          s   ){rm_samplers.Release(s);}
    void Release(Mesh *             r   ){rm_mesh.Release(r);}
};//class RenderResource
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
