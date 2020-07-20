#ifndef HGL_GRAPH_TEXT_LAYOUT_INCLUDE
#define HGL_GRAPH_TEXT_LAYOUT_INCLUDE

#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/RenderableInstance.h>
#include<hgl/graph/RenderableCreater.h>
#include<hgl/type/RectScope.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 文本排版处理
         */
        class TextLayout
        {
            FontSource *source;

        public:

            enum class Direction
            {
                LeftToRight=0,          ///<横排从左到右
                RightToLeft,            ///<横排从右到左
                TopToRight,             ///<坚排从上到下从右到左
            };//enum class Direction

            RectScope2f scope;          ///<绘制区域

            bool text_symbols;          ///<是否开启文本到符号自动替换
            bool symbols_edge_disable;  ///<符号边界禁用

        public:
        
            TextLayout(FontSource *fs):source(fs){}
            virtual ~TextLayout()=default;

            bool Layout(const UTF16String &);
        };//class TextLayout
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_LAYOUT_INCLUDE
