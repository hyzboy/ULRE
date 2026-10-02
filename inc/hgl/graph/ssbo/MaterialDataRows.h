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

    /**
     * 材质数据**组件行**（B4-iii-a：(Global, MaterialData) 行 arena 的 C++ 唯一真源）
     *
     * 与上面三个「内容行」分工不同：
     *   · 内容行（PBRSurface / EmissiveSurface / TransmissionSurface）= **被引用的资源行**，
     *     shader 按 `MTL_ROW(…)` 读它的字段；
     *   · 本行 = **组件行**：每份材质数据一行，行内只说「我引用哪一行内容」+ 授权标记。
     *     组件（`ecs::MaterialData`）只持本行行号，不再持每实体副本（B4-i 已删死字段）。
     *
     * 归属：**设备级**（`GraphicsContext` 拥有，与 `GlobalSSBOBufferRegistry` 同域）——
     * 材质数据是资源级、跨世界共享的，行号必须全局可解析（B4 拍板「路 B」）。
     * 行 0 = 哨兵（「无材质数据」）；池 `reserve_rows = 1` 保证它不被 Acquire。
     */
    struct MaterialDataRow
    {
        uint32 surface_type       = 0;              ///< GlobalSSBOType 值（PBR / Emissive / Transmission）
        uint32 surface_ssbo_id    = 0;              ///< 内容池 id（= GlobalSSBOBinding::ssbo_id）
        uint32 surface_data_index = uint32(-1);     ///< 内容行号（= GlobalSSBOBinding::data_index）
        uint32 authored_flags     = 0;              ///< 见 kMaterialDataFlag* 位常量
    };
    static_assert(sizeof(MaterialDataRow) % 16 == 0);
    static_assert(sizeof(MaterialDataRow) == 16);

    /// `MaterialDataRow::authored_flags`：已授权材质数据行（= 旧 `MaterialDataAuthoringResource::authored`）
    inline constexpr uint32 kMaterialDataFlagAuthoredDataResource = 1u << 0;

    static_assert(sizeof(PBRSurfaceRow)                %16==0);
    static_assert(sizeof(EmissiveSurfaceRow)           %16==0);
    static_assert(sizeof(TransmissionSurfaceRow)       %16==0);
    static_assert(sizeof(PBRSurfaceRow)==32);
    static_assert(sizeof(EmissiveSurfaceRow)==16);
    static_assert(sizeof(TransmissionSurfaceRow)==16);
}//namespace hgl::graph::ssbo
