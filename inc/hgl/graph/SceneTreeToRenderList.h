#ifndef HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE
#define HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE

#include<hgl/graph/RenderList.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
namespace hgl
{
    namespace graph
    {
        struct RenderNode
        {
            SceneNode *node;
        };//

        class SceneTreeToRenderList
        {
            using PipelineSets=Sets<Pipeline *>;
            using MaterialSets=Sets<Material *>;
            using MatInstanceSets=Sets<MaterialInstance *>;

        protected:

            GPUDevice *device;

        protected:

            Camera *        camera;
            CameraInfo *    camera_info;
            Frustum         frustum;

        protected:

            SceneNodeList * scene_node_list;        ///<场景节点列表

            PipelineSets    pipeline_sets;          ///<管线合集
            MaterialSets    material_sets;          ///<材质合集
            MatInstanceSets mat_instance_sets;      ///<材质实例合集

            RenderList *    render_list;

        protected:

            virtual float   CameraLength(SceneNode *,SceneNode *);                                  ///<摄像机距离比较函数

            virtual bool    InFrustum(const SceneNode *,void *);                                    ///<平截头截剪函数

            virtual bool    Begin();
            virtual bool    Expend(SceneNode *);
            virtual bool    End();

        public:

            SceneTreeToRenderList(GPUDevice *d)
            {
                device=d;
                camera=nullptr;
                camera_info=nullptr;

                scene_node_list=nullptr;
                render_list=nullptr;
            }

            virtual ~SceneTreeToRenderList();

            virtual bool    Expend(RenderList *,Camera *,SceneNode *);
        };//class SceneTreeToRenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE
