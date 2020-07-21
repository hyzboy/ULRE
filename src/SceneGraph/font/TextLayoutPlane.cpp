#include<hgl/graph/font/TextLayout.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 平面文本排版
         */
        int PlaneTextLayout(RenderableCreater *rc,FontSource *font_source,const int max_chars,const UTF16String &str)
        {
            if(!rc
             ||!font_source
             ||max_chars<=0
             ||str.IsEmpty())
                return 0;

            
        }
    }//namespace graph
}//namespace hgl