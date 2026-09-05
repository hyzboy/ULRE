#pragma once

#include<hgl/CoreType.h>
#include<hgl/color/Color4f.h>
#include<hgl/graph/ssbo/TextureSlot.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<cstddef>

namespace hgl::graph::ssbo
{
    /**
     * 材质实例数据行结构（Arena+BDA 路径的 C++ 唯一真源）
     *
     * 每个类型一行 = 材质数据 + 统一 10 槽 bindless 纹理句柄尾（tex_），
     * scalar layout 与 GLSL 侧 buffer_reference 行声明逐字节一致
     * （GLSL 声明由 ShaderGen 依 MaterialSSBOLayout 表发射）。
     *
     * 约束：sizeof(Row) % 16 == 0（Arena 16B 块粒度，static_assert 强制）。
     * 行大小在 BDA 寻址下无对齐/步长约束，非 16 整数倍部分以 reserved 补齐。
     */

    /**
     * 统一纹理句柄尾：索引即 TextureSlot 枚举值，0 = 无句柄。
     * GLSL 侧字段名为 tex_<snake_case 槽名>（GetTextureSlotName）。
     */
    struct MaterialDataRowTexTail
    {
        uint32 tex[(size_t)mtl::TextureSlot::RANGE_SIZE];

        uint32 &operator[](const mtl::TextureSlot slot)      { return tex[(uint32)slot]; }
        uint32  operator[](const mtl::TextureSlot slot) const{ return tex[(uint32)slot]; }
    };

    struct PBRSurfaceRow
    {
        Color4f base_color   = Color4f(1.0f);   ///<基础颜色
        float   metallic     = 0.0f;            ///<金属度
        float   roughness    = 1.0f;            ///<粗糙度
        float   normal_scale = kDefaultLitMaterialNormalStrength;    ///<法线强度
        float   fresnel      = kDefaultLitMaterialFresnel;           ///<菲涅尔

        MaterialDataRowTexTail tex_tail;

        uint32 reserved0[2];                    ///<补齐至 16B 整倍数

        uint32 &Tex(const mtl::TextureSlot slot)     { return tex_tail[slot]; }
        uint32  Tex(const mtl::TextureSlot slot)const{ return tex_tail[slot]; }
    };//struct PBRSurfaceRow: 80B = 5 blocks

    struct EmissiveSurfaceRow
    {
        Color4f color = Color4f(1.0f);          ///<自发光颜色

        MaterialDataRowTexTail tex_tail;

        uint32 reserved0[2];                    ///<补齐至 16B 整倍数
    };//struct EmissiveSurfaceRow: 64B = 4 blocks

    struct TextureRectArraySurfaceRow
    {
        uint32 id[4];                           ///<uvec4 矩形数组 id

        MaterialDataRowTexTail tex_tail;

        uint32 reserved0[2];                    ///<补齐至 16B 整倍数
    };//struct TextureRectArraySurfaceRow: 64B = 4 blocks

    struct TransmissionSurfaceRow
    {
        uint32 trans_color;                     ///<透射色（打包）

        MaterialDataRowTexTail tex_tail;

        uint32 reserved0[1];                    ///<补齐至 16B 整倍数
    };//struct TransmissionSurfaceRow: 48B = 3 blocks

    static_assert(sizeof(PBRSurfaceRow)                %16==0);
    static_assert(sizeof(EmissiveSurfaceRow)           %16==0);
    static_assert(sizeof(TextureRectArraySurfaceRow)   %16==0);
    static_assert(sizeof(TransmissionSurfaceRow)       %16==0);

    static_assert(sizeof(PBRSurfaceRow)==80);
    static_assert(offsetof(PBRSurfaceRow,tex_tail)==32);
}//namespace hgl::graph::ssbo
