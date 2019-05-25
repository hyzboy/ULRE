#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/type/Color4f.h>
namespace hgl
{
    namespace graph
    {
        class RenderableNode;

        struct UBOMatrixData
        {
            Matrix4f projection;
            Matrix4f modelview;
            Matrix4f mvp;
            Matrix3f normal;
        };//

        struct UBOSkyLight
        {            
            Color4f sun_color;
            Vector4f sun_direction;
        };//

        class RenderList
        {
            vulkan::CommandBuffer *cmd_buf;

        private:

            Camera camera;

            Frustum frustum;

        private:

            UBOMatrixData ubo_matrix;
            UBOSkyLight ubo_skylight;

        private:

            List<RenderableNode *> renderable_node_list;

            vulkan::Pipeline *      last_pipeline;
            vulkan::DescriptorSets *last_desc_sets;
            vulkan::Renderable *    last_renderable;

            void Render(RenderableNode *,const Matrix4f &);

        public:

            RenderList()
            {
                cmd_buf=nullptr;
                last_pipeline=nullptr;
                last_desc_sets=nullptr;
                last_renderable=nullptr;
            }

            ~RenderList()=default;

            void Add    (RenderableNode *node)  {if(node)renderable_node_list.Add(node);}
            void Clear  ()                      {renderable_node_list.ClearData();}

            void SetCamera(const Camera &);
            void SetSkyLightColor(const Color4f &c,const Vector4f &d)
            {
                ubo_skylight.sun_color=c;
                ubo_skylight.sun_direction=d;
            }

            bool Render();
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
