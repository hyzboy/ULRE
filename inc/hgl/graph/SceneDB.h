#ifndef HGL_GRAPH_SCENE_DATABASE_INCLUDE
#define HGL_GRAPH_SCENE_DATABASE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/ResManage.h>
namespace hgl
{
    namespace graph
    {
        using MaterialID        =int;
        using PipelineID        =int;
        using DescriptorSetsID  =int;
        using RenderableID      =int;
        using BufferID          =int;

        /**
         * 场景DB，用于管理场景内所需的所有数据
         */
        class SceneDB
        {
            IDResManage<MaterialID,         vulkan::Material>       rm_material;                    ///<材质合集
            IDResManage<PipelineID,         vulkan::Pipeline>       rm_pipeline;                    ///<管线合集
            IDResManage<DescriptorSetsID,   vulkan::DescriptorSets> rm_desc_sets;                   ///<描述符合集
            IDResManage<RenderableID,       vulkan::Renderable>     rm_renderables;                  ///<可渲染对象合集
            IDResManage<BufferID,           vulkan::Buffer>         rm_buffers;                     ///<顶点缓冲区集合

        public:

            SceneDB()=default;
            virtual ~SceneDB()=default;

            MaterialID          Add(vulkan::Material *      mtl ){return rm_material.Add(mtl);}
            PipelineID          Add(vulkan::Pipeline *      p   ){return rm_pipeline.Add(p);}
            DescriptorSetsID    Add(vulkan::DescriptorSets *ds  ){return rm_desc_sets.Add(ds);}
            RenderableID        Add(vulkan::Renderable *    r   ){return rm_renderables.Add(r);}
            BufferID            Add(vulkan::Buffer *        buf ){return rm_buffers.Add(buf);}

            vulkan::Material *      GetMaterial     (const MaterialID       &id){return rm_material.Get(id);}
            vulkan::Pipeline *      GetPipeline     (const PipelineID       &id){return rm_pipeline.Get(id);}
            vulkan::DescriptorSets *GetDescSets     (const DescriptorSetsID &id){return rm_desc_sets.Get(id);}
            vulkan::Renderable *    GetRenderable   (const RenderableID     &id){return rm_renderables.Get(id);}
            vulkan::Buffer *        GetBuffer       (const BufferID         &id){return rm_buffers.Get(id);}
        };//class SceneDB
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_DATABASE_INCLUDE
