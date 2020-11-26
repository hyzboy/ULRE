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
        template<typename T> struct GPUArray
        {
            List<T> list;
            GPUBuffer *buffer;

        public:

            GPUArray()
            {
                buffer=nullptr;
            }

            ~GPUArray()
            {
                SAFE_CLEAR(buffer);
            }

            void Clear()
            {
                list.ClearData();
            }

            void Add(const T &data)
            {
                list.Add(data);
            }
        };//

        class RenderList
        {
            RenderCmdBuffer *cmd_buf;

        private:

            GPUArray<Matrix4f> LocalToWorld;

            List<SceneNode *> scene_node_list;

            Pipeline *          last_pipeline;
            MaterialInstance *  last_mat_inst;
            RenderableInstance *last_ri;

            void Render(SceneNode *,RenderableInstance *);
            void Render(SceneNode *,List<RenderableInstance *> &);

        public:

            RenderList();
            ~RenderList()=default;
            
            void Begin  ();
            void Add    (SceneNode *);
            void End    ();

            bool Render (RenderCmdBuffer *);
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
