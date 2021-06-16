#ifndef HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE
#define HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE

#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialParameters.h>

using RenderNodePointer=hgl::graph::RenderNode *;

using RenderNodeComparator=Comparator<RenderNodePointer>;

namespace hgl
{
    namespace graph
    {
        using MVPArrayBuffer=GPUArrayBuffer<MVPMatrix>;

        class SceneTreeToRenderList
        {
            //using PipelineSets  =Sets<Pipeline *>;
            using MaterialSets  =Sets<Material *>;
            //using MatInstSets   =Sets<MaterialParameters *>;

        protected:

            GPUDevice *     device;

        protected:

            CameraInfo      camera_info;            ///<相机信息
            Frustum         frustum;

        protected:
        
            RenderNodeComparator render_node_comparator;

            RenderNodeList  render_node_list;       ///<场景节点列表

            //PipelineSets    pipeline_sets;          ///<管线合集
            MaterialSets    material_sets;          ///<材质合集
            //MatInstSets     mat_inst_sets;          ///<材质实例合集

            需将以下两行数据转到RenderList，避免这边释放信息就没了。two_triangle测试这个
            MVPArrayBuffer *mvp_array;
            List<RenderableInstance *> ri_list;

            RenderList *    render_list;

        protected:

            virtual bool    Begin();
            virtual bool    Expend(SceneNode *);
            virtual void    End();

        public:

            SceneTreeToRenderList(GPUDevice *d);
            virtual ~SceneTreeToRenderList();

            virtual bool    Expend(RenderList *,const CameraInfo &,SceneNode *);
        };//class SceneTreeToRenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE
