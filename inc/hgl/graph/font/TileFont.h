#ifndef HGL_GRAPH_TILE_FONT_INCLUDE
#define HGL_GRAPH_TILE_FONT_INCLUDE

#include<hgl/graph/TileData.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/type/UnicodeBlocks.h>

namespace hgl
{
    namespace graph
    {
        /**
         * Tile字符管理<br>
         * 本模块允许有多个字符数据来源，每个来源也可以对应多个unicode块, 但一个unicode块只能对应一个字体数据来源
         */
        class TileFont
        {
            FontSource *source;
            TileData *tile_data;

        public:

            TileFont(TileData *td,FontSource *fs);
            virtual ~TileFont();
        };//class TileFont
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_FONT_INCLUDE
