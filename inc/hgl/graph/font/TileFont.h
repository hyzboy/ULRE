#ifndef HGL_GRAPH_TILE_FONT_INCLUDE
#define HGL_GRAPH_TILE_FONT_INCLUDE

#include<hgl/graph/TileData.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/type/RefObjectPool.h>

namespace hgl
{
    namespace graph
    {
        using TileResPool=RefObjectPool<u32char,TileObject *>;

        /**
         * Tile字符管理<br>
         * 本模块允许有多个字符数据来源，每个来源也可以对应多个unicode块, 但一个unicode块只能对应一个字体数据来源
         */
        class TileFont
        {
            FontSource *source;
            TileData *tile_data;

            TileResPool to_res;

            SortedSet<u32char> not_bitmap_chars;

        public:

            FontSource *GetFontSource   (){return source;}
            TileData *  GetTileData     (){return tile_data;}
            Texture2D * GetTexture      (){return tile_data->GetTexture();}

        public:

            TileFont(TileData *td,FontSource *fs);
            virtual ~TileFont();

            bool Registry(TileUVFloatMap &,SortedSet<u32char> &chars_sets);                         ///<注册要使用的字符
            void Unregistry(const DataArray<u32char> &);                                            ///<注销要使用的字符
        };//class TileFont
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_FONT_INCLUDE
