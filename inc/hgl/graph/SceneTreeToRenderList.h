#ifndef HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE
#define HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE

#include<hgl/graph/RenderList.h>
namespace hgl
{
    namespace graph
    {
        class SceneTreeToRenderList
        {
            using PipelineSets=Sets<Pipeline *>;
            using MaterialSets=Sets<Material *>;
            using MatInstanceSets=Sets<MaterialInstance *>;

        protected:

            GPUDevice *device;

        protected:

            Camera *        camera;
            CameraMatrix *  camera_matrix;
            Frustum         frustum;

        protected:

            SceneNodeList * scene_node_list;        ///<�����ڵ��б�

            PipelineSets    pipeline_sets;          ///<���ߺϼ�
            MaterialSets    material_sets;          ///<���ʺϼ�
            MatInstanceSets mat_instance_sets;      ///<����ʵ���ϼ�

            RenderList *    render_list;

        protected:

            virtual uint32  CameraLength(SceneNode *,SceneNode *);                                  ///<���������ȽϺ���

            virtual bool    InFrustum(const SceneNode *,void *);                                    ///<ƽ��ͷ�ؼ�����

            virtual bool    Begin();
            virtual bool    Expend(SceneNode *);
            virtual bool    End();

        public:

            SceneTreeToRenderList(GPUDevice *d)
            {
                device=d;
                camera=nullptr;
                camera_matrix=nullptr;

                scene_node_list=nullptr;
                render_list=nullptr;
            }

            virtual ~SceneTreeToRenderList();

            virtual bool    Expend(RenderList *,Camera *,SceneNode *);
        };//class SceneTreeToRenderList        
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_SCENE_TREE_TO_RENDER_LIST_INCLUDE
