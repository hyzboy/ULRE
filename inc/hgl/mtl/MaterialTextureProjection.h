#pragma once

#include<hgl/mtl/MaterialRecipe.h>

namespace hgl::graph::mtl
{
    // ═══════════════════════════════════════════════════════════════════════════
    // 材质源贴图的**投影表示**（projection）
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // `base_color` 槽描述的是"一张贴图"，但采样它需要知道**方向怎么变成采样坐标**：
    //
    //   Texture2D  → 直接用 uv0（普通贴图 / 图集 / 2D 数组的常规情况）
    //   CubeMap    → 方向直接喂 samplerCube（`cubefile:` → cube array 视图）
    //   Octahedral → 方向先做八面体逆映射得到 uv，再当普通 2D 图采样
    //                （TexConv 的八面体展开图，扩展名 `.TexOct`）
    //
    // 三者**共用同一份主着色器**：领域源码里 `SampleMaterialColor` 的调用点不变，
    // 差别只落在 `SampleMaterialProjected` 的实现模块上——由 `EmitDomainOptions` 多发
    // 一行 `#include` 选中，因此主 `.frag` 里没有任何 `#if`/`#elif` 分支。
    //
    // **不新增材质领域**：领域描述"表面模型"（无光纯贴图就只有 UnlitTexture3D），
    // 投影描述"同一张槽里的资源形态"。同一颗球换成八面体图不需要换领域、不需要换
    // 配方里的 domain，只换资源。
    //
    // 为什么判据是**路径后缀**而不是 `SamplerType`：槽声明的
    // `SamplerType::Sampler2DArray` 只说明"怎么进 bindless 数组"，而 2D 图既可能是
    // 普通贴图、图集，也可能是八面体展开图；`Texture2DAtlas` / `Texture2DArray`
    // 后续还会进来，凭"是 2D"推断八面体必然误判。所以八面体必须有**显式标记**
    // （`.TexOct` 扩展名）才认，其余一律 Texture2D。
    enum class MaterialTextureProjection : unsigned char
    {
        Texture2D = 0,   ///< 常规 2D（含 2D 数组 / 图集）：按 uv0 采样
        CubeMap,         ///< Cubemap：按方向采样
        Octahedral,      ///< 八面体展开的 2D 图：方向 → 八面体 uv → 按 uv0 采样
    };

    /// 承担上述投影语义的槽名。目前只有无光单图材质源（`material_source_texture`）
    /// 消费它；PBR 的多槽材质源仍然是纯 uv 采样，不参与投影选择。
    inline constexpr const char *MaterialSourceTextureSlotName = "base_color";

    /// Sky 域的贴图槽名。它同样走投影选择（`surface_sky_cube` 调
    /// `SampleMaterialProjected`），因此天空背景换 Cube / 八面体 / 2D 展开图只换资源，
    /// 不换领域、不换表面模块、不换主着色器。
    /// 纯解析大气（`surface_sky_minimal`）没有这个槽，自然保持 Texture2D。
    inline constexpr const char *SkyTextureSlotName = "sky_cube";

    /// 由资源的 `resource_id` 判定投影表示（大小写不敏感的后缀判定）：
    ///   `.TexCube` → CubeMap；`.TexOct` → Octahedral；其余（含空串 / 资产 ID / 图集）→ Texture2D。
    /// 前缀（`file:` / `cubefile:` / `texfile:` / `texcubefile:`）不参与判定：
    /// 前缀决定"怎么装载"，后缀决定"装载出来是什么形态"，两者是独立事实。
    MaterialTextureProjection ResolveMaterialTextureProjection(const char *resource_id) noexcept;

    /// 从配方取 `base_color` / `sky_cube` 槽的投影表示。配方没有这两个槽 ⇒ Texture2D
    /// （历史行为：纯 2D 材质与纯解析天空都不发射投影 include）。
    ///
    /// 只看 `resource_id`。用**句柄**（`RecipeTextureBinding::asset`）绑定且没有
    /// `resource_id` 的配方一律判为 Texture2D —— 句柄路径拿到的是 OSString，跨平台
    /// 宽度不同，且在烘焙/发行两条路径上的就绪时机不一致；需要八面体时请在配方里
    /// 写明确的 URI（`texfile:...TexOct`），这是**显式标记**本来就要求的。
    MaterialTextureProjection ResolveMaterialSourceProjection(const MaterialRecipe &recipe) noexcept;

    /// 投影表示 → `SampleMaterialProjected` 的实现模块路径（字面相对路径，供 options 发射）。
    /// 三份实现共用同一个 include guard，因此"默认 2D + options 覆盖"不会撞名。
    /// 返回 nullptr = 未知枚举值。
    const char *GetMaterialTextureProjectionIncludePath(MaterialTextureProjection projection) noexcept;

    /// 投影表示 → 稳定字符串（诊断/日志用）。
    const char *GetMaterialTextureProjectionName(MaterialTextureProjection projection) noexcept;
}// namespace hgl::graph::mtl
