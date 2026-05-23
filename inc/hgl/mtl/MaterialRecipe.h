#pragma once

/// MaterialRecipe.h — 可序列化材质资产描述符
///
/// 这是一个纯数据结构（允许 std::string），用于描述一个完整的材质创建参数。
/// 目前作为"硬编码常量表"在示例程序中使用，模拟从外部文件（JSON/BIN）加载材质配置。
/// 后续只需替换填充该结构体的数据来源，即可无缝切换到真正的外部序列化方案。
///
/// 内部逻辑分段（ShaderGen 视角）：
///   DrawSpec       — 决定"选哪个 shader"：preset / dim / prim / vertex_policy / position_provider
///   ResourceSupply — 决定"能提供哪些资源"：textures / ubos / sky_available
///   AlphaConfig    — 决定"alpha 怎么处理"：mode / source / threshold / tex_slot / pcg_path
///
/// MaterialRecipe 将上述所有字段展平到单个结构体中，
/// 通过 dim 字段区分 2D / 3D，通过 preset 字段决定子类型处理逻辑。

#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialFeature.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/SamplerSlot.h>
#include <hgl/mtl/RenderAlphaMode.h>
#include <hgl/mtl/MaterialRenderState.h>
#include <hgl/shadergen/ColorSource.h>
#include <hgl/common/CoordinateSystem.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/common/TextureSamplerTypeDef.h>
#include <hgl/mtl/MaterialVariantRow.h>
#include <hgl/mtl/ShaderDataSchema.h>
#include <hgl/common/PositionProvider.h>

#include <vector>
#include <string>

namespace hgl::graph::mtl
{

// ─────────────────────────────────────────────────────────────────────────────
// AlphaConfig — recipe 层的 alpha 描述（输入端）
// 运行时由 Matcher 转换为 MatchedShaderSet::alpha_overlay（输出端）。
// ─────────────────────────────────────────────────────────────────────────────
struct AlphaConfig
{
    enum class Mode : uint8_t {
        Opaque = 0,
        Masked,          ///< discard by threshold
        Transparent,     ///< src-alpha blend
        Dither,          ///< ordered dither
        AlphaToCoverage,
    };

    enum class Source : uint8_t {
        ConstantOne = 0, ///< alpha = 1.0（不需要额外 tex）
        BaseColorAlpha,  ///< 从 BaseColor 贴图 .a 通道读取
        SeparateTex,     ///< 专用 alpha 贴图（tex_slot 指向）
        PCGFn,           ///< 程序化计算（pcg_path 非空）
    };

    Mode        mode      = Mode::Opaque;
    Source      source    = Source::ConstantOne;
    float       threshold = 0.5f;       ///< 仅 Masked / Dither 有意义
    SamplerSlot tex_slot  = SamplerSlot::Opacity; ///< 仅 Source==SeparateTex 有意义
    std::string pcg_path;               ///< ShaderLibrary 相对路径；仅 Source==PCGFn 非空
};

// ─────────────────────────────────────────────────────────────────────────────
// MaterialRecipe — 材质资产描述符（可序列化的平铺结构体）
// ─────────────────────────────────────────────────────────────────────────────

/// 设计原则：
/// - 与序列化层解耦：字段全部是基础类型或 std::string，无运行时指针
/// - 2D/3D 共用一个结构体，通过 dim 字段区分；未使用的字段在对应分支被忽略
/// - 纹理路径为空时跳过加载；未使用的字段在对应分支被忽略
struct MaterialRecipe
{
    // ════════════════════════════════════════════════════════════════════════
    // 标识
    // ════════════════════════════════════════════════════════════════════════
    std::string     id;         ///< 逻辑资产名（缓存键 / 文件名干）
    std::string     domain_id;  ///< 资源域标识（同 ShaderMaterialProgram 不同域 = 不同渲染批次）

    // ════════════════════════════════════════════════════════════════════════
    // DrawSpec — 决定"选哪个 shader"
    // ════════════════════════════════════════════════════════════════════════
    MaterialPreset  preset  = MaterialPreset::Standard;  ///< 材质预设

    /// 作者意图特性集合（位掩码）
    /// - 0 表示使用 preset 默认映射
    /// - 非 0 表示覆盖默认映射（由上层显式声明需求）
    MaterialFeatureMask intent_features = 0;

    /// 是否严格要求 intent_features 与最终程序特征一致。
    /// - false: 允许运行时降级/回退重写（推荐默认）
    /// - true:  供调试/验证链路使用
    bool strict_intent_features = false;

    /// 维度（2D / 3D）
    enum class Dim : uint8 { D2, D3 } dim = Dim::D3;

    PrimitiveType   prim = PrimitiveType::Triangles;  ///< 图元类型

    /// 顶点变换策略。Unknown = 由 preset alias 展开决定（兼容旧路径）。
    VertexTransformPolicy vertex_policy = VertexTransformPolicy::Unknown;

    /// 顶点位置来源。Unknown = 由 preset 表决定。
    /// 设为 UserPCG 时须同时填写 user_provider_path。
    hgl::graph::PositionProviderId position_provider = hgl::graph::PositionProviderId::Unknown;

    /// 用户自定义顶点位置提供器的 GLSL 路径（相对于 ShaderLibrary）。
    /// position_provider == UserPCG 时有效；preset 必须为 Custom。
    std::string user_provider_path;

    /// 表面着色模型。Unknown = 由 preset alias 展开决定（兼容旧路径）。
    SurfaceShadingModel   shading_model = SurfaceShadingModel::Unknown;

    /// schema 轴。默认不启用显式覆盖，保持 preset-only 旧行为。
    ShaderDataSchema schema = ShaderDataSchema::None;
    bool has_explicit_schema = false;

    // ── 2D 专用（dim == D2 时有效）────────────────────────────────────────────
    CoordinateSystem2D coord_2d = CoordinateSystem2D::NDC;

    // ── 3D 专用（dim == D3 时有效）────────────────────────────────────────────
    SkyLightAmbientModel sky_ambient = SkyLightAmbientModel::Simple;

    // ════════════════════════════════════════════════════════════════════════
    // ResourceSupply — 决定"能提供哪些资源"
    // ════════════════════════════════════════════════════════════════════════

    /// 资源需求轴。默认不启用显式覆盖，保持 preset-only 旧行为。
    MaterialResourceRequirements resources{};
    bool has_explicit_resources = false;

    // ════════════════════════════════════════════════════════════════════════
    // AlphaConfig — 决定"alpha 怎么处理"
    // ════════════════════════════════════════════════════════════════════════
    AlphaConfig alpha_config{};

    // ════════════════════════════════════════════════════════════════════════
    // 渲染状态 & 编译 hint
    // ════════════════════════════════════════════════════════════════════════

    /// 资源级默认渲染状态。离线烘焙的输入基准。
    /// 运行时可被 PrimitiveComponent::user_rs_override 或 TransitionState 覆盖。
    MaterialRenderState default_render_state{};

    /// 烘焙 hint：告知离线烘焙器额外为哪些 pass 生成 SPV 变体。
    PrecompileHints precompile_hints{};

    // ════════════════════════════════════════════════════════════════════════
    // 颜色/纹理来源声明（ColorSource / PCG 统一化路径）
    // ════════════════════════════════════════════════════════════════════════
    std::vector<graph::ColorSource> color_sources;

    // ── 纹理资产引用（仅用于资产层加载，不影响 shader routing）───────────────
    /// slot+path 对，供 MaterialRecipeRegistry 加载纹理到 DomainResourceBinding。
    /// shader routing / source_mode 已由 color_sources[] 承担，此处无需再存。
    struct TextureAssetRef
    {
        SamplerSlot  slot = SamplerSlot::BaseColor;
        std::string  path;  ///< 空字符串 = 不加载贴图
    };
    std::vector<TextureAssetRef> textures;

}; // struct MaterialRecipe

} // namespace hgl::graph::mtl
