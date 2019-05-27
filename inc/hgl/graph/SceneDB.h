#ifndef HGL_GRAPH_SCENE_DATABASE_INCLUDE
#define HGL_GRAPH_SCENE_DATABASE_INCLUDE

#include<hgl/graph/vulkan/VKMaterial.h>
#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/graph/vulkan/VKDescriptorSets.h>
#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/VertexBufferCreater.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/type/ResManage.h>
namespace hgl
{
    namespace graph
    {
        using MaterialID        =int;
        using PipelineID        =int;
        using BufferID          =int;
        using DescriptorSetsID  =int;
        using RenderableID          =int;
        using RenderableInstanceID  =int;

        class VertexBufferCreater;

        /**
         * 场景DB，用于管理场景内所需的所有数据
         */
        class SceneDB
        {
            vulkan::Device *device;

            IDResManage<MaterialID,             vulkan::Material>       rm_material;                ///<材质合集
            IDResManage<PipelineID,             vulkan::Pipeline>       rm_pipeline;                ///<管线合集
            IDResManage<DescriptorSetsID,       vulkan::DescriptorSets> rm_desc_sets;               ///<描述符合集
            IDResManage<RenderableID,           vulkan::Renderable>     rm_renderables;             ///<可渲染对象合集
            IDResManage<BufferID,               vulkan::Buffer>         rm_buffers;                 ///<顶点缓冲区合集
            IDResManage<RenderableInstanceID,   RenderableInstance>     rm_renderable_instances;    ///<渲染实例集合集

        public:

            SceneDB(vulkan::Device *dev):device(dev){}
            virtual ~SceneDB()=default;

            MaterialID              Add(vulkan::Material *      mtl ){return rm_material.Add(mtl);}
            PipelineID              Add(vulkan::Pipeline *      p   ){return rm_pipeline.Add(p);}
            DescriptorSetsID        Add(vulkan::DescriptorSets *ds  ){return rm_desc_sets.Add(ds);}
            RenderableID            Add(vulkan::Renderable *    r   ){return rm_renderables.Add(r);}
            BufferID                Add(vulkan::Buffer *        buf ){return rm_buffers.Add(buf);}
            RenderableInstanceID    Add(RenderableInstance *    ri  ){return rm_renderable_instances.Add(ri);}

            vulkan::VertexBuffer *CreateVBO(VkFormat format,uint32_t count,const void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
            vulkan::VertexBuffer *CreateVBO(VkFormat format,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateVBO(format,count,nullptr,sharing_mode);}
            vulkan::VertexBuffer *CreateVBO(const VertexBufferCreater *vbc,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateVBO(vbc->GetDataType(),vbc->GetCount(),vbc->GetData(),sharing_mode);}

            #define SCENE_DB_CREATE_FUNC(name)  vulkan::Buffer *Create##name(VkDeviceSize size,void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);   \
                                                vulkan::Buffer *Create##name(VkDeviceSize size,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);

                SCENE_DB_CREATE_FUNC(UBO)
                SCENE_DB_CREATE_FUNC(SSBO)
                SCENE_DB_CREATE_FUNC(INBO)

            #undef SCENE_DB_CREATE_FUNC


            vulkan::IndexBuffer *CreateIBO(VkIndexType index_type,uint32_t count,const void *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE);
            vulkan::IndexBuffer *CreateIBO16(uint32_t count,const uint16 *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT16,count,(void *)data,sharing_mode);}
            vulkan::IndexBuffer *CreateIBO32(uint32_t count,const uint32 *data,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT32,count,(void *)data,sharing_mode);}

            vulkan::IndexBuffer *CreateIBO(VkIndexType index_type,uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(index_type,count,nullptr,sharing_mode);}
            vulkan::IndexBuffer *CreateIBO16(uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT16,count,nullptr,sharing_mode);}
            vulkan::IndexBuffer *CreateIBO32(uint32_t count,VkSharingMode sharing_mode=VK_SHARING_MODE_EXCLUSIVE){return CreateIBO(VK_INDEX_TYPE_UINT32,count,nullptr,sharing_mode);}

            RenderableInstance *    CreateRenderableInstance(vulkan::Pipeline *p,vulkan::DescriptorSets *ds,vulkan::Renderable *r);

            vulkan::Material *      GetMaterial             (const MaterialID           &id){return rm_material.Get(id);}
            vulkan::Pipeline *      GetPipeline             (const PipelineID           &id){return rm_pipeline.Get(id);}
            vulkan::DescriptorSets *GetDescSets             (const DescriptorSetsID     &id){return rm_desc_sets.Get(id);}
            vulkan::Renderable *    GetRenderable           (const RenderableID         &id){return rm_renderables.Get(id);}
            vulkan::Buffer *        GetBuffer               (const BufferID             &id){return rm_buffers.Get(id);}
            RenderableInstance *    GetRenderableInstance   (const RenderableInstanceID &id){return rm_renderable_instances.Get(id);}
        };//class SceneDB
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_DATABASE_INCLUDE
