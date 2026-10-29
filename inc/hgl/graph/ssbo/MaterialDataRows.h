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
    struct HGL_SSBO_ROW MaterialDataRow
    {
        uint32 surface_type       = 0;              ///< GlobalSSBOType 值（PBR / Emissive / Transmission）
        uint32 surface_ssbo_id    = 0;              ///< 内容池 id（= GlobalSSBOBinding::ssbo_id）
        uint32 surface_data_index = uint32(-1);     ///< 内容行号（= GlobalSSBOBinding::data_index）
        uint32 authored_flags     = 0;              ///< 见 kMaterialDataFlag* 位常量
    };
    static_assert(sizeof(MaterialDataRow) % 16 == 0);
    static_assert(sizeof(MaterialDataRow) == 16);

    /// ── 「材质行号」口径（B4-ii 钉死；改这里之前先读）────────────────────────────
    /// ① **组件行** = `MaterialDataRow`：行号 = `ecs::MaterialData::material_row_id`，
    ///    池 = `GlobalSSBOType::MaterialData`（设备级、跨世界共享），**行 0 = 「无材质数据」哨兵**；
    /// ② **内容行** = `PBRSurfaceRow` / `EmissiveSurfaceRow` / `TransmissionSurfaceRow`：
    ///    由组件行三元组定位 `{surface_type, surface_ssbo_id, surface_data_index}`
    ///    （`surface_data_index == uint32(-1)` ⇒ 无内容；行号只在 `surface_type` 对应的池内有效）。
    /// 读行一律走 `GlobalSSBOBufferRegistry::GetTypedRow<T>(type, row_id)`（越界/行宽不符 ⇒ nullptr），
    /// **不要**把别的池的行指针强转成 T。

    /// `MaterialDataRow::authored_flags`：低 8 位 = **授权位**，高 24 位 = **行内 revision**（授权代数）。
    /// revision 让「只读行」的消费者也能检测变更（原先只能读组件副本）。
    inline constexpr uint32 kMaterialDataFlagAuthoredDataResource = 1u << 0;   ///< 已授权材质数据行（= 旧 authored）
    inline constexpr uint32 kMaterialDataFlagAuthoredRecipe       = 1u << 1;   ///< 有配方覆盖
    inline constexpr uint32 kMaterialDataFlagAuthoredTexture      = 1u << 2;   ///< 有具名纹理资源

    inline constexpr uint32 kMaterialDataFlagBits      = 8u;
    inline constexpr uint32 kMaterialDataFlagMask      = (1u << kMaterialDataFlagBits) - 1u;
    inline constexpr uint32 kMaterialDataRevisionShift = kMaterialDataFlagBits;
    inline constexpr uint32 kMaterialDataRevisionMask  = 0xFFFFFFFFu >> kMaterialDataFlagBits;

    /// 打包「授权位 + 行内 revision」（位段不重叠，均由 static_assert 兜住）
    inline constexpr uint32 PackMaterialDataAuthoredFlags(uint32 flags, uint32 revision) noexcept
    {
        return (flags & kMaterialDataFlagMask)
             | ((revision & kMaterialDataRevisionMask) << kMaterialDataRevisionShift);
    }

    /// 取行内 revision（= `ecs::MaterialData::GetAuthoredGeneration()` 的行内镜像）
    inline constexpr uint32 GetMaterialDataRowRevision(const MaterialDataRow &row) noexcept
    {
        return (row.authored_flags >> kMaterialDataRevisionShift) & kMaterialDataRevisionMask;
    }

    /// 取行内授权位
    inline constexpr uint32 GetMaterialDataRowFlags(const MaterialDataRow &row) noexcept
    {
        return row.authored_flags & kMaterialDataFlagMask;
    }

    static_assert(kMaterialDataFlagMask < (1u << kMaterialDataRevisionShift),
                  "授权位与行内 revision 的位段不得重叠");

    static_assert(sizeof(PBRSurfaceRow)                %16==0);
    static_assert(sizeof(EmissiveSurfaceRow)           %16==0);
    static_assert(sizeof(TransmissionSurfaceRow)       %16==0);
    static_assert(sizeof(PBRSurfaceRow)==32);
    static_assert(sizeof(EmissiveSurfaceRow)==16);
    static_assert(sizeof(TransmissionSurfaceRow)==16);
}//namespace hgl::graph::ssbo
