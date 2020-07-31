#ifndef HGL_GRAPH_TILE_FONT_INCLUDE
#define HGL_GRAPH_TILE_FONT_INCLUDE

#include<hgl/graph/TileData.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/type/ResPoolManage.h>

namespace hgl
{
    namespace graph
    {
        class TileObjectPool
        {
            TileData *tile_data;

        public:

            TileObject *Acquire();
            void Release(TileObject *);

        public:

            TileObjectPool(TileData *td):tile_data(td);
        };

        using TileObjectManage=_ResPoolManage<u32char,TileObject *,TileObjectPool>;

        /**
         * Tile字符管理<br>
         * 本模块允许有多个字符数据来源，每个来源也可以对应多个unicode块, 但一个unicode块只能对应一个字体数据来源
         */
        class TileFont
        {
            FontSource *source;
            TileData *tile_data;

            TileObjectPool *tile_pool;
            TileObjectManage *ch_tile_pool;

        public:

            FontSource *GetFontSource   (){return source;}
            TileData *  GetTileData     (){return tile_data;}

        public:

            TileFont(TileData *td,FontSource *fs);
            virtual ~TileFont();

            bool Registry(List<RectScope2f> &,const List<u32char> &);                               ///<注册要使用的字符
            void Unregistry(const List<u32char> &);                                                 ///<注销要使用的字符
        };//class TileFont
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TILE_FONT_INCLUDE
