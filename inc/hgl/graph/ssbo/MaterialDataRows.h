#pragma once

#include<hgl/CoreType.h>
#include<hgl/color/Color4f.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<cstddef>

namespace hgl::graph::ssbo
{
    /**
     * 材质实例数据行结构（Arena+BDA 路径的 C++ 唯一真源）
     *
     * 每个类型一行只包含材质业务数据。纹理 descriptor/layer 引用由
     * MaterialTextureReferencePool 独立提供，不再嵌入 payload 行。
     *
     * 约束：sizeof(Row) % 16 == 0（Arena 16B 块粒度，static_assert 强制）。
     * GLSL 声明由 ShaderGen 依据 MaterialSSBOLayout 表发射。
     */

    struct PBRSurfaceRow
    {
        Color4f base_color   = Color4f(1.0f);   ///<基础颜色
        float   metallic     = 0.0f;            ///<金属度
        float   roughness    = 1.0f;            ///<粗糙度
        float   normal_scale = kDefaultLitMaterialNormalStrength;    ///<法线强度
        float   fresnel      = kDefaultLitMaterialFresnel;           ///<菲涅尔
    };//struct PBRSurfaceRow: 32B = 2 blocks

    struct EmissiveSurfaceRow
    {
        Color4f color = Color4f(1.0f);          ///<自发光颜色
    };//struct EmissiveSurfaceRow: 16B = 1 block

    struct TransmissionSurfaceRow
    {
        uint32 trans_color;                     ///<透射色（打包）
        uint32 reserved0[3];                    ///<补齐至 16B 整倍数
    };//struct TransmissionSurfaceRow: 16B = 1 block

    static_assert(sizeof(PBRSurfaceRow)                %16==0);
    static_assert(sizeof(EmissiveSurfaceRow)           %16==0);
    static_assert(sizeof(TransmissionSurfaceRow)       %16==0);
    static_assert(sizeof(PBRSurfaceRow)==32);
    static_assert(sizeof(EmissiveSurfaceRow)==16);
    static_assert(sizeof(TransmissionSurfaceRow)==16);
}//namespace hgl::graph::ssbo
