#ifndef HGL_GRAPH_FONT_MANAGE_INCLUDE
#define HGL_GRAPH_FONT_MANAGE_INCLUDE

#include<hgl/graph/font/Font.h>
#include<hgl/type/Map.h>
namespace hgl
{
    namespace graph
    {
        struct FontConfig
        {
        };//

        /**
         * 字体管理模块，包括所有的字体数据源，以及字符绘制器。<br>
         * 在可期的未来，我们会增加对字体、字符的使用情况统计信息
         */
        class FontManage
        {
            MapObject<Font,FontSource> sources;

            TileFont *
        };//class FontManage
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_MANAGE_INCLUDE
