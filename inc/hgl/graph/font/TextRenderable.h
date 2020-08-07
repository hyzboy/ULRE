#ifndef HGL_GRAPH_TEXT_RENDERABLE_INCLUDE
#define HGL_GRAPH_TEXT_RENDERABLE_INCLUDE

#include<hgl/graph/vulkan/VKRenderable.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 文本可渲染对象
         */
        class TextRenderable:public vulkan::Renderable
        {
            vulkan::Device *device;

            vulkan::Renderable *render_obj;

            uint32 max_count;                                                   ///<缓冲区最大容量
            uint32 cur_count;                                                   ///<当前容量

            vulkan::VAB *vab_vertex;
            vulkan::VAB *vab_tex_coord;

        public:

            TextRenderable(vulkan::Device *,int mc=1024);
            virtual ~TextRenderable();

            void SetMaxCount(int);
        };//class TextRenderable:public vulkan::Renderable
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_RENDERABLE_INCLUDE
