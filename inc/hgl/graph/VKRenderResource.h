#ifndef HGL_GRAPH_VULKAN_DATABASE_INCLUDE
#define HGL_GRAPH_VULKAN_DATABASE_INCLUDE

#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKSampler.h>
#include<hgl/graph/VKTexture.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/Mesh.h>
#include<hgl/type/ObjectManage.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

class MaterialManager;

namespace mtl
{
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
}//namespace mtl

using MaterialID            =int;
using MaterialInstanceID    =int;
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
    MaterialManager *material_manager;
    
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
    MaterialManager *GetMaterialManager(){return material_manager;}

public:

    RenderResource(VulkanDevice *dev, MaterialManager *mtl_mgr):device(dev),material_manager(mtl_mgr){}
    virtual ~RenderResource()=default;

public: //添加数据到管理器（如果指针为nullptr会返回-1）

    MaterialID              Add(Material *          mtl ){return material_manager->Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return material_manager->Add(mi);}

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

public: //Material

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name,const ShaderCreateInfo *sci)
    {
        return material_manager->CreateShaderModule(shader_module_name, sci);
    }
    
    Material *          CreateMaterial  (const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci)
    {
        return material_manager->CreateMaterial(mtl_name, mci);
    }       ///<基于名称创建一个材质(一般用于程序内嵌材质)

    Material *          LoadMaterial    (const AnsiString &mtl_name,mtl::Material2DCreateConfig *cfg)
    {
        return material_manager->LoadMaterial(mtl_name, cfg);
    }         ///<基于资产名称加载一个材质(一般用于从文件加载材质)
    Material *          LoadMaterial    (const AnsiString &mtl_name,mtl::Material3DCreateConfig *cfg)
    {
        return material_manager->LoadMaterial(mtl_name, cfg);
    }

    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VIL *vil)
    {
        return material_manager->CreateMaterialInstance(mtl, vil);
    }
    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg=nullptr)
    {
        return material_manager->CreateMaterialInstance(mtl, vil_cfg);
    }

    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VIL *vil,const void *data,const int bytes)
    {
        return material_manager->CreateMaterialInstance(mtl, vil, data, bytes);
    }
    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const void *data,const int bytes)
    {
        return material_manager->CreateMaterialInstance(mtl, vil_cfg, data, bytes);
    }

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VIL *vil,const T *data)
    {
        return material_manager->CreateMaterialInstance(mtl, vil, data);
    }

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl,const VILConfig *vil_cfg,const T *data)
    {
        return material_manager->CreateMaterialInstance(mtl, vil_cfg, data);
    }

    MaterialInstance *  CreateMaterialInstance(const AnsiString &mtl_name,const mtl::MaterialCreateInfo *mci,const VILConfig *vil_cfg=nullptr)
    {
        return material_manager->CreateMaterialInstance(mtl_name, mci, vil_cfg);
    }

    Mesh *              CreateMesh(Primitive *r,MaterialInstance *mi,Pipeline *p);
    Mesh *              CreateMesh(PrimitiveCreater *pc,MaterialInstance *mi,Pipeline *p);

    Sampler *           CreateSampler(VkSamplerCreateInfo *sci=nullptr);
    Sampler *           CreateSampler(Texture *);

public: //Get

    Material *          GetMaterial         (const MaterialID           &id){return material_manager->GetMaterial(id);}
    MaterialInstance *  GetMaterialInstance (const MaterialInstanceID   &id){return material_manager->GetMaterialInstance(id);}

    Primitive *         GetPrimitive        (const PrimitiveID          &id){return rm_primitives.Get(id);}
    DeviceBuffer *      GetBuffer           (const BufferID             &id){return rm_buffers.Get(id);}
    Sampler *           GetSampler          (const SamplerID            &id){return rm_samplers.Get(id);}
    Mesh *              GetMesh             (const MeshID               &id){return rm_mesh.Get(id);}

public: //Release

    void Release(Material *         mtl ){material_manager->Release(mtl);}
    void Release(MaterialInstance * mi  ){material_manager->Release(mi);}

    void Release(Primitive *        p   ){rm_primitives.Release(p);}
    void Release(DeviceBuffer *     buf ){rm_buffers.Release(buf);}
    void Release(Sampler *          s   ){rm_samplers.Release(s);}
    void Release(Mesh *             r   ){rm_mesh.Release(r);}
};//class RenderResource
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DATABASE_INCLUDE
