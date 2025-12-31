#pragma once

#include<hgl/type/RectScope.h>
#include<hgl/type/Map.h>

namespace hgl::graph
{
    using namespace hgl::math;

    struct TileObject
    {
        int id;

        int col,row;            //当前tile在整个纹理中的tile位置

        RectScope2i uv_pixel;   //以象素为单位的tile位置和尺寸
        RectScope2f uv_float;   //以浮点为单位的tile位置和尺寸
    };

    using TileUVFloat=RectScope2f;
    using TileUVFloatMap=Map<u32char,RectScope2f>;
}//hgl::namespace graph
