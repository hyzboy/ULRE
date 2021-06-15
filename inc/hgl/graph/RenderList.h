#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/SceneInfo.h>
#include<hgl/type/Color4f.h>
#include<hgl/type/Sets.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 渲染对象列表<br>
         * 已经展开的渲染对象列表，产生mvp用UBO/SSBO等数据，最终创建RenderCommandBuffer
         */
        class RenderList
        {
            RenderCmdBuffer *cmd_buf;

        private:

            Camera *camera;

            GPUBuffer *mvp_buffer;
            List<RenderableInstance *> *ri_list;

            DescriptorSets *renderable_desc_sets;

            uint32_t ubo_offset;
            uint32_t ubo_align;

        private:

            Pipeline *          last_pipeline;
            MaterialInstance *  last_mi;
            uint32_t            last_vbo;

            void Render(RenderableInstance *);

        private:

            friend class SceneTreeToRenderList;

            void Set(List<RenderableInstance *> *,GPUBuffer *,const uint32_t);

        public:

            RenderList();
            virtual ~RenderList()=default;
            
            bool Render(RenderCmdBuffer *);
        };//class RenderList        
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
