#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/type/Color4f.h>
namespace hgl
{
    namespace graph
    {
        class RenderableInstance;

        class RenderList
        {
            vulkan::CommandBuffer *cmd_buf;

        private:

            List<SceneNode *> scene_node_list;

            vulkan::PushConstant *      last_pc;
            vulkan::Pipeline *          last_pipeline;
            vulkan::MaterialInstance *  last_mat_inst;
            vulkan::Renderable *        last_renderable;

            void Render(SceneNode *,RenderableInstance *);
            void Render(SceneNode *,List<RenderableInstance *> &);

        public:

            RenderList()
            {
                cmd_buf=nullptr;
                last_pc=nullptr;
                last_pipeline=nullptr;
                last_mat_inst=nullptr;
                last_renderable=nullptr;
            }

            ~RenderList()=default;

            void Add    (SceneNode *node)  {if(node)scene_node_list.Add(node);}
            void Clear  ()                 {scene_node_list.ClearData();}

            bool Render(vulkan::CommandBuffer *);
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
