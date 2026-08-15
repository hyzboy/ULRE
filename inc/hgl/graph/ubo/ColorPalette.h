#pragma once

#include <hgl/CoreType.h>

namespace hgl::graph
{
    /**
     * 顶点调色板 UBO（全局 Scene 集，Set 0, binding=3）。
     *
     * GPU 端以 uint32[256] 存储（RGBA8 打包），GLSL 端用 unpackUnorm4x8 解码。
     * 打包顺序为 LSB=r..MSB=a（即 Color4f::toABGR8()），与 unpackUnorm4x8 的
     * xyzw=(r,g,b,a) 对应。
     */
    struct ColorPalette
    {
        static constexpr int kSize = 256;

        uint32 color[kSize];
    };
}
