#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/type/Color4f.h>
namespace hgl
{
    namespace graph
    {
        struct MVPArray;

        class RenderList
        {
            GPUDevice *device;
            RenderCmdBuffer *cmd_buf;

        private:

            Camera *camera;

            MVPArray *mvp_array;

            List<SceneNode *> scene_node_list;

            Pipeline *          last_pipeline;
            RenderableInstance *last_ri;

            void Render(SceneNode *,RenderableInstance *);
            void Render(SceneNode *,List<RenderableInstance *> &);

        private:

            friend class GPUDevice;

            RenderList(GPUDevice *);

        public:

            virtual ~RenderList();
            
            void Begin  ();
            void Add    (SceneNode *);
            void End    (CameraMatrix *);

            bool Render (RenderCmdBuffer *);
        };//class RenderList
        
        class SceneTreeToRenderList
        {
            GPUDevice *device;

        public:

            Camera *        camera;
            CameraMatrix *  camera_matrix;
            Frustum         frustum;

        public:

            virtual uint32  CameraLength(SceneNode *,SceneNode *);                                  ///<摄像机距离比较函数

            virtual bool    InFrustum(const SceneNode *,void *);                                    ///<平截头截剪函数

        public:

            SceneTreeToRenderList(GPUDevice *d)
            {
                device=d;
                camera=nullptr;
                camera_matrix=nullptr;
            }

            virtual bool    Expend(RenderList *,Camera *,SceneNode *);
        };//class SceneTreeToRenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
