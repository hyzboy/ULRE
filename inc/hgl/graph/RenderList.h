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
        struct L2WArrays;

        class RenderList
        {
            GPUDevice *device;
            RenderCmdBuffer *cmd_buf;

        private:

            L2WArrays *LocalToWorld;

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
            void End    ();

            bool Render (RenderCmdBuffer *);
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
