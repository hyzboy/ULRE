#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/Camera.h>
namespace hgl
{
    namespace graph
    {
        class SceneNode;

        struct UBOMatrixData
        {
            Matrix4f projection;
            Matrix4f modelview;
            Matrix4f mvp;
        };//

        struct UBOSkyLight
        {
            Vector4f sky_color;
        };//

        class RenderList
        {
            vulkan::CommandBuffer *cmd_buf;

            Camera camera;

            Matrix4f projection_matrix,
                     modelview_matrix,
                     mvp_matrix;

            Frustum frustum;

        private:

            List<const SceneNode *> SceneNodeList;

            vulkan::Pipeline *      last_pipeline;
            vulkan::DescriptorSets *last_desc_sets;
            vulkan::Renderable *    last_renderable;

            void Render(vulkan::RenderableInstance *,const Matrix4f &);

        public:

            RenderList()
            {
                cmd_buf=nullptr;
                last_pipeline=nullptr;
                last_desc_sets=nullptr;
                last_renderable=nullptr;
            }

            ~RenderList()=default;

            void Add    (const SceneNode *node) {if(node)SceneNodeList.Add(node);}
            void Clear  ()                      {SceneNodeList.ClearData();}

            void SetCamera(const Camera &);

            bool Render();
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
