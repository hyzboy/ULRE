#ifndef HGL_GRAPH_TEXT_LAYOUT_INCLUDE
#define HGL_GRAPH_TEXT_LAYOUT_INCLUDE

#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/VertexBuffer.h>
namespace hgl
{
    namespace graph
    {

        class TextRenderable:public vulkan::Renderable
        {
        };//class TextRenderable:public vulkan::Renderable

        /**
         * 文本排版处理
         */
        class TextLayout
        {
            FontSource *source;

        public:
        
            TextLayout(FontSource *fs):source(fs){}
            virtual ~TextLayout()=default;


        };//class TextLayout
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_LAYOUT_INCLUDE
