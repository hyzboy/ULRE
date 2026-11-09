#pragma once

#include<hgl/CoreType.h>
#include<hgl/color/Color4f.h>
#include<hgl/graph/BufferAnnotation.h>
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

    struct HGL_SSBO_ROW PBRSurfaceRow
    {
        Color4f base_color   = Color4f(1.0f);   ///<基础颜色
        float   metallic     = 0.0f;            ///<金属度
        float   roughness    = 1.0f;            ///<粗糙度
        float   normal_scale = kDefaultLitMaterialNormalStrength;    ///<法线强度
        float   fresnel      = kDefaultLitMaterialFresnel;           ///<菲涅尔
    };//struct PBRSurfaceRow: 32B = 2 blocks

    struct HGL_SSBO_ROW EmissiveSurfaceRow
    {
        Color4f color = Color4f(1.0f);          ///<自发光颜色
    };//struct EmissiveSurfaceRow: 16B = 1 block

    struct HGL_SSBO_ROW TransmissionSurfaceRow
    {
        uint32 trans_color;                     ///<透射色（打包）
        uint32 reserved0[3];                    ///<补齐至 16B 整倍数
    };//struct TransmissionSurfaceRow: 16B = 1 block

    /**
     * Filament 高级材质扩展行（ClearCoat / Sheen / Anisotropy / Subsurface）。
     *
     * 前 32B 与 `PBRSurfaceRow` **逐字节同构**（base_color / metallic / roughness /
     * normal_scale / fresnel）：同一份基础材质数据可以原样升级进本行，字段偏移
     * 不漂移。后 32B 是高级参数；`sheen_color` 与 `TransmissionSurfaceRow::trans_color`
     * 同款打包（`packUnorm4x8`，授权侧是 `Color4f`）。
     */
    struct HGL_SSBO_ROW PBRAdvancedSurfaceRow
    {
        Color4f base_color   = Color4f(1.0f);   ///<基础颜色（与 PBRSurfaceRow 同偏移）
        float   metallic     = 0.0f;            ///<金属度
        float   roughness    = 1.0f;            ///<粗糙度
        float   normal_scale = kDefaultLitMaterialNormalStrength;    ///<法线强度
        float   fresnel      = kDefaultLitMaterialFresnel;           ///<菲涅尔
        float   clearcoat    = 0.0f;            ///<清漆强度（KHR_materials_clearcoat）
        float   clearcoat_roughness = 0.0f;     ///<清漆层粗糙度
        float   sheen_roughness     = 0.0f;     ///<绒毛粗糙度（KHR_materials_sheen）
        float   anisotropy          = 0.0f;     ///<各向异性强度（KHR_materials_anisotropy）
        uint32  sheen_color         = 0u;       ///<绒毛色（打包 unorm4x8，rgb 有效）
        float   subsurface_power    = 0.0f;     ///<次表面 wrap 权重（0 = 关闭）
        float   thickness           = 0.0f;     ///<厚度（背光穿透衰减）
        uint32  reserved0           = 0u;       ///<补齐至 64B 整倍数
    };//struct PBRAdvancedSurfaceRow: 64B = 4 blocks

    // ── 「材质行号」口径（2026-11 收敛后）────────────────────────────────────────
    /// **内容行** = `PBRSurfaceRow` / `EmissiveSurfaceRow` / `TransmissionSurfaceRow` / `PBRAdvancedSurfaceRow`：
    ///    由 `GlobalSSBOBinding` 三元组定位 `{ssbo_type, ssbo_id, data_index}`
    ///    （`data_index == uint32(-1)` ⇒ 无内容；行号只在 `ssbo_type` 对应的池内有效）。
    /// 读行一律走 `GlobalSSBOBufferRegistry::GetTypedRow<T>(type, row_id)`（越界/行宽不符 ⇒ nullptr），
    /// **不要**把别的池的行指针强转成 T。
    ///
    ///（原「组件行 `MaterialDataRow` + kMaterialDataFlag* 授权位/行内 revision」已整体退役：
    ///  GPU 从未消费该池；授权状态改由 `ecs::MaterialAuthoringSideTable`（键 = EntityID）承载，
    ///  授权代数即条目里的 `authored_generation`。）

    static_assert(sizeof(PBRSurfaceRow)                %16==0);
    static_assert(sizeof(EmissiveSurfaceRow)           %16==0);
    static_assert(sizeof(TransmissionSurfaceRow)       %16==0);
    static_assert(sizeof(PBRAdvancedSurfaceRow)        %16==0);
    static_assert(sizeof(PBRSurfaceRow)==32);
    static_assert(sizeof(EmissiveSurfaceRow)==16);
    static_assert(sizeof(TransmissionSurfaceRow)==16);
    static_assert(sizeof(PBRAdvancedSurfaceRow)==64);
    // 前 32B 与 PBRSurfaceRow 逐字段同构（偏移不得漂移）
    static_assert(offsetof(PBRAdvancedSurfaceRow, base_color) == offsetof(PBRSurfaceRow, base_color));
    static_assert(offsetof(PBRAdvancedSurfaceRow, metallic)   == offsetof(PBRSurfaceRow, metallic));
    static_assert(offsetof(PBRAdvancedSurfaceRow, roughness)  == offsetof(PBRSurfaceRow, roughness));
    static_assert(offsetof(PBRAdvancedSurfaceRow, normal_scale)== offsetof(PBRSurfaceRow, normal_scale));
    static_assert(offsetof(PBRAdvancedSurfaceRow, fresnel)    == offsetof(PBRSurfaceRow, fresnel));
}//namespace hgl::graph::ssbo
