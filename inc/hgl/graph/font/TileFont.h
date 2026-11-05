#pragma once

#include<hgl/graph/tile/TileData.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/type/RefObjectPool.h>

namespace hgl::graph
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

        U32CharSet not_bitmap_chars;

        TileObject  *solid_tile=nullptr;      ///<纯色 Tile(画线/画块用)，常驻不回收
        TileUVFloat  solid_uv;                ///<纯色 Tile 的采样 UV(退化为 tile 中心，杜绝边缘渗色)

    public:

        FontSource *GetFontSource   (){return source;}
        TileData *  GetTileData     (){return tile_data;}
        Texture2D * GetTexture      (){return tile_data->GetTexture();}

        /// 图集是否已生成纯色 Tile
        bool HasSolidTile()const{return solid_tile!=nullptr;}

        /**
        * 取得纯色 Tile 的采样 UV。
        * 四个角退化为 tile 中心同一点：双线性/三线性采样恒读到内部 0xFF，
        * 不会采到相邻字形 tile 的透明 spread 边缘，故无需 shader 改动即可画纯色块。
        * @return 纯色 tile 存在时返回其 UV 矩形指针，否则 nullptr
        */
        const TileUVFloat *GetSolidTileUV(){return solid_tile?&solid_uv:nullptr;}

    public:

        TileFont(TileData *td,FontSource *fs);
        virtual ~TileFont();

        bool Registry(TileUVFloatMap &,U32CharSet &chars_sets);                                 ///<注册要使用的字符
        void Unregistry(const std::vector<u32char> &);                                            ///<注销要使用的字符
    };//class TileFont
}//namespace hgl::graph
