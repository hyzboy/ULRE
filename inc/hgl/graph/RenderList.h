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
        class RenderList
        {
            RenderCommand *cmd_buf;

        private:

            List<SceneNode *> scene_node_list;

            PushConstant *      last_pc;
            Pipeline *          last_pipeline;
            MaterialInstance *  last_mat_inst;
            RenderableInstance *last_ri;

            void Render(SceneNode *,RenderableInstance *);
            void Render(SceneNode *,List<RenderableInstance *> &);

        public:

            RenderList()
            {
                cmd_buf=nullptr;
                last_pc=nullptr;
                last_pipeline=nullptr;
                last_mat_inst=nullptr;
                last_ri=nullptr;
            }

            ~RenderList()=default;

            void Add    (SceneNode *node)  {if(node)scene_node_list.Add(node);}
            void Clear  ()                 {scene_node_list.ClearData();}

            bool Render (RenderCommand *);
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
