/// ShaderComposition.h — 合成驱动型着色器体系
///
/// 设计原理：
///   1. 开发者只写核心算法片段（VertexShader、FragmentShader）
///   2. 框架根据渲染上下文（光照模型、输出类型、阶段）自动填充通用部分
///   3. 最终 GLSL = 前置宏 + 结构体定义 + 输入获取 + [业务代码] + 输出合成
///
/// 例如开发者的业务代码只需：
///   VertexShader() {
///       return GetLocalPosition() * GetLocalToWorldMatrix();
///   }
///
/// 框架会自动：
///   - 根据 VBO/SSBO/Computed 选择输入方式
///   - 插入坐标变换（L2W + Camera VP）
///   - 插入光照计算（根据 LightModel 枚举选择 Lambert/PBR 等）
///   - 根据 OutputMode 决定混合、Alpha 还是加色输出

#pragma once

#include <hgl/mtl/FixedVertexEntry.h>
#include <hgl/mtl/FixedDescriptorEntry.h>
#include <hgl/mtl/ShaderPermutationKey.h>
#include <hgl/common/RenderFlowDef.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/type/String.h>
#include <string>
#include <vector>

namespace hgl::graph::mtl {

constexpr uint32_t ShaderGenStageVertex = 0x00000001u;
constexpr uint32_t ShaderGenStageFragment = 0x00000010u;
constexpr uint32_t ShaderGenStageAllGraphics = 0x0000001Fu;

struct MaterialLogicDef;

// ─────────────────────────────────────────────────────────────────────────────
// 着色器段定义（开发者编写的业务逻辑）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * 顶点着色器业务段
 * 输入：VertexInput 结构（由框架定义）
 * 输出：VS_Output 结构（VS output 接口）
 *
 * 典型实现：
 *   vec4 VertexShaderBusiness(const VertexInput vi) {
 *       return GetLocalToWorld() * vec4(vi.Position, 1.0);
 *   }
 */
struct VertexShaderBusiness {
    const char *code;  ///< 业务代码片段（含函数定义）
    // 业务函数签名约定：vec4 VertexShaderBusiness(const VertexInput vi)
};

/**
 * 片元着色器业务段
 * 输入：VS_Output（来自 VS 插值）
 * 输出：vec3 diffuse_color, vec3 specular_color, float alpha（或其他）
 *
 * 典型实现：
 *   vec3 FragmentShaderBusiness(const VS_Output vso) {
 *       return texture(diffuse_map, vso.TexCoord).rgb;
 *   }
 */
struct FragmentShaderBusiness {
    const char *code;  ///< 业务代码片段（含函数定义）
    // 业务函数签名约定：vec3/vec4 FragmentShaderBusiness(const VS_Output vso)
};

// ─────────────────────────────────────────────────────────────────────────────
// 输出模式（框架根据此决定最终 RT 合成方式）
// ─────────────────────────────────────────────────────────────────────────────

enum class ShaderOutputMode : uint8 {
    /// 单一 RT：RGB = color，A = opacity（默认）
    /// 输出公式：finalColor = color * (1 - alpha) + bgColor * alpha
    SingleRTAlphaBlend = 0,

    /// 单一 RT：RGB = color，A = alpha（预乘混合）
    /// 输出：finalColor = color + bgColor * (1 - alpha)
    SingleRTPremultiplied,

    /// 单一 RT：RGB = color 加式，忽略 alpha
    /// 输出：finalColor = color + bgColor（适合光效、爆炸）
    SingleRTAdditive,

    /// 双 RT（Forward+ / 延迟渲染 G-Buffer）
    /// RT0 = diffuse color (RGB)
    /// RT1 = specular color (RGB)
    DualRTDeferred,

    /// 自定义输出（由特定材质自行决定 RT count）
    Custom,

    ENUM_CLASS_RANGE(SingleRTAlphaBlend, Custom)
};

// 渲染流程/管线/覆盖模式等“渲染器 + ShaderGen 共用定义”已迁移到
// `hgl/common/RenderFlowDef.h`，此处仅保留 ShaderComposition 专属定义。

struct ShaderComposeDiagnostics {
    bool normal_compression_policy_normalized = false;
    bool normal_policy_normalized_vertex_input = false;
    bool normal_policy_normalized_normal_map = false;
    bool normal_policy_normalized_gbuffer = false;

    // Helper 冲突诊断（业务代码自定义 helper 与 builtin helper 发生重名）
    bool helper_conflict_detected = false;
    uint32_t helper_conflict_count = 0;
    std::vector<std::string> helper_conflicts;
};

struct ShaderComposeResult {
    std::string code;
    ShaderComposeDiagnostics diagnostics;
};

// ─────────────────────────────────────────────────────────────────────────────
// 光照计算委托（框架根据 ShaderPermutationKey 生成）
// ─────────────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────────────
// 合成描述符（材质开发者定义）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * ComposedMaterialDef — 合成驱动材质定义
 *
 * 用途：代替 FixedMaterialDef 的低级 GLSL 编写，提供更高级的抽象。
 *
 * 框架生成流程：
 *   1. 读取 vertex_business + fragment_business
 *   2. 根据 output_mode 生成输出合成代码（Alpha / Additive / G-Buffer output）
 *   3. 根据 lighting_enabled + ShaderPermutationKey 生成光照部分（Lambert / PBR / IBL）
 *   4. 拼接完整 GLSL：
 *      ```glsl
 *      #version 450
 *      #define LIGHT_MODEL 3  // 由 ShaderPermutationKey 注入
 *      ...
 *      [layout 声明 + uniform 声明]
 *      [结构体定义：VertexInput, VS_Output, LightingOutput]
 *      [坐标变换函数]
 *      [业务片段]
 *      [光照计算片段]
 *      [main()]
 *      ```
 *   5. 编译到 SPV
 */
struct ComposedMaterialDef {
    const char *name;

    PrimitiveType primitive_type;

    /// 顶点输入和描述符（同 FixedMaterialDef）
    const FixedVertexEntry *vertex_entries;
    uint32_t vertex_entry_count;
    const FixedDescriptorEntry *descriptor_entries;
    uint32_t descriptor_entry_count;

    /// 业务着色器段
    const VertexShaderBusiness *vertex_business;
    const FragmentShaderBusiness *fragment_business;

    /// 输出模式
    ShaderOutputMode output_mode;

    /// 是否启用光照计算（若 true，框架根据 ShaderPermutationKey 生成光照代码）
    bool enable_lighting;

    /// 材质实例数据
    const char *mi_glsl_codes;
    uint32_t mi_struct_bytes;

    /// 可选：显式 helper 依赖（Stage 5+）
    /// 当配置后，框架会按依赖名注入 helper；未配置时回退到业务代码字符串检测。
    const char **vertex_required_helpers;
    uint32_t vertex_required_helper_count;

    const char **fragment_required_helpers;
    uint32_t fragment_required_helper_count;

    /// 逻辑驱动的 helper 依赖（来自 ShaderLogic.h 的 required_helpers）
    std::vector<std::string> logic_required_helpers;
};

struct LogicResourceBridgeDiagnostics {
    std::vector<std::string> missing_resources;
};

struct ComposedMaterialBuildFromLogicResult {
    ComposedMaterialDef def;
    VertexShaderBusiness vertex_business;
    FragmentShaderBusiness fragment_business;
    std::vector<FixedDescriptorEntry> filtered_descriptors;
    LogicResourceBridgeDiagnostics diagnostics;

    bool HasMissingResources() const { return !diagnostics.missing_resources.empty(); }
};

bool BuildComposedMaterialDefFromLogic(
    const ComposedMaterialDef &base_def,
    const MaterialLogicDef &logic,
    ComposedMaterialBuildFromLogicResult &out);

// ─────────────────────────────────────────────────────────────────────────────
// 辅助函数库生成策略：自动生成开发者使用的工具函数
// ─────────────────────────────────────────────────────────────────────────────

/**
 * HelperFunctionLibrary — 框架自动生成的开发者辅助函数库
 *
 * 框架根据 ComposedMaterialDef 的信息（顶点输入、描述符、坐标系）
 * 自动生成如下函数，开发者无需关心实现细节，直接调用即可：
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 坐标变换相关                                                  │
 * ├─────────────────────────────────────────────────────────────┤
 * │ mat4 GetLocalToWorld()                                        │
 * │   来自于 descriptor LocalToWorld (UBO/SSBO)                  │
 * │   自动选择：ByIndex / ByAssign / Fixed                       │
 * │                                                               │
 * │ mat3 GetNormalMatrix()                                        │
 * │   = transpose(inverse(mat3(ViewMatrix * LocalToWorld)))      │
 * │   框架自动从 LocalToWorld 推导                               │
 * │                                                               │
 * │ vec4 GetPosition3D()                                          │
 * │   VS: 返回 LocalToWorld * vec4(Position, 1.0)               │
 * │   GS/FS: 返回 插值的 WorldPosition                          │
 * │                                                               │
 * │ vec4 GetClipPosition()                                        │
 * │   = camera.vp * GetPosition3D()                              │
 * │   框架根据坐标系自动完成                                      │
 * └─────────────────────────────────────────────────────────────┘
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 法线相关                                                      │
 * ├─────────────────────────────────────────────────────────────┤
 * │ vec3 GetNormal(vec3 local_normal)                            │
 * │ vec3 GetNormal()  [VS 版本，直接用 Normal 输入]             │
 * │   = normalize(GetNormalMatrix() * local_normal)              │
 * │   框架自动选择 VS/GS/FS 版本                                │
 * │                                                               │
 * │ vec3 GetWorldNormal()  [GS/FS，从 VS 输入]                  │
 * │   = normalize(所插值的 WorldNormal)                         │
 * └─────────────────────────────────────────────────────────────┘
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │ 材质实例相关                                                  │
 * ├─────────────────────────────────────────────────────────────┤
 * │ MaterialInstance GetMaterialInstance()                        │
 * │ MaterialInstance GetMI()                                      │
 * │   VS: 从 MaterialInstanceID 读取（SSBO 或直接索引）         │
 * │   GS: 从 Input[0].MaterialInstanceID 读取                    │
 * │   FS: 从 Input.MaterialInstanceID 读取（来自 VS 插值）      │
 * │   框架自动选择正确的版本                                      │
 * │                                                               │
 * │ void HandoverMaterialInstanceID()  [仅用于有 GS 时]         │
 * │   = 在 GS 中转发 MaterialInstanceID                          │
 * │   框架根据 shader stage 决定是否生成                         │
 * └─────────────────────────────────────────────────────────────┘
 *
 * 开发者编写业务逻辑时，直接调用这些函数即可：
 *
 *   // VS 中
 *   vec4 VertexShaderBusiness(const VertexInput vi) {
 *       vec3 world_normal = GetNormal(vi.Normal);  // 框架自动处理矩阵
 *       vec4 world_pos = GetLocalToWorld() * vec4(vi.Position, 1.0);
 *       return GetClipPosition();  // 框架自动投影
 *   }
 *
 *   // FS 中
 *   vec4 FragmentShaderBusiness(const VS_Output vso) {
 *       MaterialInstance mi = GetMaterialInstance();  // 自动从 SSBO 读
 *       vec3 normal = GetWorldNormal();  // 从插值数据获取
 *       return mi.Color;
 *   }
 */
struct HelperFunctionLibrary {
    // 框架生成的完整函数库代码
    std::string code;
};

// ═════════════════════════════════════════════════════════════════════════════
// 预定义配置（编译期常量）
// ═════════════════════════════════════════════════════════════════════════════

/**
 * GBufferConfigurations — GBuffer 配置预设表
 *
 * 所有画质档位的配置都在此预定义。
 * ShaderGen 遍历此表生成所有 GBuffer 变体的 Shader。
 */
namespace GBufferConfigurations {
    // ─── 低/中档配置 ───

    inline const GBufferConfiguration Low = {
        GBufferQualityPreset::Low,
        GBufferFormatLevel::MobileLite,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth,
        false,  // no motion vector
        { false, NormalEncodingMode::Octahedral, false, NormalEncodingMode::None, false, NormalEncodingMode::None },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::Low,
            GBufferFormatLevel::MobileLite,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth,
            false,
            { false, NormalEncodingMode::Octahedral, false, NormalEncodingMode::None, false, NormalEncodingMode::None })
    };

    inline const GBufferConfiguration LowPlus = {
        GBufferQualityPreset::LowPlus,
        GBufferFormatLevel::MobileExtended,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Emissive,
        false,
        { true, NormalEncodingMode::Octahedral, false, NormalEncodingMode::None, false, NormalEncodingMode::None },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::LowPlus,
            GBufferFormatLevel::MobileExtended,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Emissive,
            false,
            { true, NormalEncodingMode::Octahedral, false, NormalEncodingMode::None, false, NormalEncodingMode::None })
    };

    inline const GBufferConfiguration Medium = {
        GBufferQualityPreset::Medium,
        GBufferFormatLevel::MobileExtended,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth |
        GBufferChannel::Emissive | GBufferChannel::MotionVector,
        true,
        { true, NormalEncodingMode::Octahedral, false, NormalEncodingMode::None, true, NormalEncodingMode::Octahedral },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::Medium,
            GBufferFormatLevel::MobileExtended,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth |
                GBufferChannel::Emissive | GBufferChannel::MotionVector,
            true,
            { true, NormalEncodingMode::Octahedral, false, NormalEncodingMode::None, true, NormalEncodingMode::Octahedral })
    };

    // ─── 中高/高档配置 ───

    inline const GBufferConfiguration MediumPlus = {
        GBufferQualityPreset::MediumPlus,
        GBufferFormatLevel::DesktopStandard,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Specular,
        false,
        { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::MediumPlus,
            GBufferFormatLevel::DesktopStandard,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Specular,
            false,
            { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None })
    };

    inline const GBufferConfiguration High = {
        GBufferQualityPreset::High,
        GBufferFormatLevel::DesktopStandard,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth |
        GBufferChannel::Specular | GBufferChannel::Roughness,
        false,
        { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::High,
            GBufferFormatLevel::DesktopStandard,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth |
                GBufferChannel::Specular | GBufferChannel::Roughness,
            false,
            { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None })
    };

    inline const GBufferConfiguration HighPlus = {
        GBufferQualityPreset::HighPlus,
        GBufferFormatLevel::DesktopFull,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Emissive |
        GBufferChannel::MotionVector | GBufferChannel::Specular | GBufferChannel::Roughness | GBufferChannel::Metallic,
        true,
        { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::HighPlus,
            GBufferFormatLevel::DesktopFull,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Emissive |
                GBufferChannel::MotionVector | GBufferChannel::Specular | GBufferChannel::Roughness | GBufferChannel::Metallic,
            true,
            { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None })
    };

    inline const GBufferConfiguration Ultra = {
        GBufferQualityPreset::Ultra,
        GBufferFormatLevel::DesktopFull,
        GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Emissive |
        GBufferChannel::MotionVector | GBufferChannel::Specular | GBufferChannel::Roughness |
        GBufferChannel::Metallic | GBufferChannel::AO,
        true,
        { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None },
        ComputeGBufferVariantHash(
            GBufferQualityPreset::Ultra,
            GBufferFormatLevel::DesktopFull,
            GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth | GBufferChannel::Emissive |
                GBufferChannel::MotionVector | GBufferChannel::Specular | GBufferChannel::Roughness |
                GBufferChannel::Metallic | GBufferChannel::AO,
            true,
            { false, NormalEncodingMode::None, false, NormalEncodingMode::None, false, NormalEncodingMode::None })
    };

    // ─── 配置索引表 ───
    inline const GBufferConfiguration* const AllConfigs[] = {
        &Low,
        &LowPlus,
        &Medium,
        &MediumPlus,
        &High,
        &HighPlus,
        &Ultra,
    };

    inline constexpr uint32_t ConfigCount = sizeof(AllConfigs) / sizeof(AllConfigs[0]);

    // ─── 查找辅助函数 ───
    inline const GBufferConfiguration* GetConfig(GBufferQualityPreset preset) {
        for (uint32_t i = 0; i < ConfigCount; ++i) {
            if (AllConfigs[i]->preset == preset)
                return AllConfigs[i];
        }
        return nullptr;
    }
}

/**
 * RenderFlows — 渲染流程预设表
 *
 * 所有渲染流程都在此预定义。
 * ShaderGen 遍历此表，结合 GBufferConfigurations，生成完整的 Shader 变体矩阵。
 */
namespace RenderFlows {
    // ═════════════════════════════════════════════════════════════════════════
    // Forward_Basic — 最基础的前向渲染
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Forward_Basic_Passes[] = {
        { RenderStage::Forward_Opaque,      true,  true,  GBufferChannel::None, GBufferChannel::Color, PipelineCoverageMode::Solid, true },
        { RenderStage::Forward_Masked,      true,  true,  GBufferChannel::None, GBufferChannel::Color, PipelineCoverageMode::Mask,  true },
        { RenderStage::Forward_Transparent, true,  false, GBufferChannel::None, GBufferChannel::Color, PipelineCoverageMode::Solid, true },
    };

    inline const RenderFlowDefinition Forward_Basic = {
        "Forward_Basic",
        RenderFlowPreset::Forward_Basic,
        Forward_Basic_Passes, 3,
        false, false,
        GBufferQualityPreset::Low,
        GBufferQualityPreset::Ultra,
        { 1, 1, false, false },
        // 迁移初期：保留双路径，默认偏向传统输入
        { VertexInputMigrationStage::DualPathValidation, true, true, true, PipelineInputMode::AutoPreferLegacy }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // Forward_WithEarlyZ — 带预深度的前向渲染
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Forward_WithEarlyZ_Passes[] = {
        { RenderStage::EarlyZ_Solid,        true,  true,  GBufferChannel::None,  GBufferChannel::Depth, PipelineCoverageMode::Solid, true  },
        { RenderStage::EarlyZ_Masked,       true,  true,  GBufferChannel::None,  GBufferChannel::Depth, PipelineCoverageMode::Mask,  false },
        { RenderStage::Forward_Opaque,      true,  false, GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Solid, true  },
        { RenderStage::Forward_Masked,      true,  false, GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Mask,  true  },
        { RenderStage::Forward_Transparent, true,  false, GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Solid, true  },
    };

    inline const RenderFlowDefinition Forward_WithEarlyZ = {
        "Forward_WithEarlyZ",
        RenderFlowPreset::Forward_WithEarlyZ,
        Forward_WithEarlyZ_Passes, 5,
        false, false,
        GBufferQualityPreset::LowPlus,
        GBufferQualityPreset::Ultra,
        { 1, 1, false, false },
        // 迁移中期：默认优先 SSBO，保留回退
        { VertexInputMigrationStage::PreferSSBO, true, true, true, PipelineInputMode::AutoPreferSSBO }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // Deferred_Standard — 标准延迟渲染
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Deferred_Standard_Passes[] = {
        { RenderStage::GBuffer_Opaque,      true,  true,  GBufferChannel::None,
          GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth, PipelineCoverageMode::Solid, true },
        { RenderStage::GBuffer_Masked,      true,  true,  GBufferChannel::None,
          GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth, PipelineCoverageMode::Mask,  true },
        { RenderStage::Deferred_Lighting,   true,  false,
          GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Solid, true },
        { RenderStage::Forward_Transparent, true,  false, GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Solid, true },
    };

    inline const RenderFlowDefinition Deferred_Standard = {
        "Deferred_Standard",
        RenderFlowPreset::Deferred_Standard,
        Deferred_Standard_Passes, 4,
        false, false,
        GBufferQualityPreset::High,
        GBufferQualityPreset::Ultra,
        { 3, 1, false, false },
        // 延迟主路径通常优先 SSBO，保留验证回退能力
        { VertexInputMigrationStage::PreferSSBO, true, true, true, PipelineInputMode::AutoPreferSSBO }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // ForwardPlus_DoubleHZB — 双重 HZB 优化的 Forward+
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition ForwardPlus_DoubleHZB_Passes[] = {
        { RenderStage::EarlyZ_Solid,        true,  true,  GBufferChannel::None,  GBufferChannel::Depth, PipelineCoverageMode::Solid, true  },
        { RenderStage::HZB_Generation,      true,  false, GBufferChannel::Depth, GBufferChannel::None,  PipelineCoverageMode::Solid, true  },
        { RenderStage::HZB_Culling,         false, false, GBufferChannel::None,  GBufferChannel::None,  PipelineCoverageMode::Solid, true  },
        { RenderStage::Forward_Opaque,      true,  false, GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Solid, true  },
        { RenderStage::HZB_Generation,      true,  false, GBufferChannel::Depth, GBufferChannel::None,  PipelineCoverageMode::Solid, true  },
        { RenderStage::Forward_Transparent, true,  false, GBufferChannel::Depth, GBufferChannel::Color, PipelineCoverageMode::Solid, true  },
    };

    inline const RenderFlowDefinition ForwardPlus_DoubleHZB = {
        "ForwardPlus_DoubleHZB",
        RenderFlowPreset::ForwardPlus_DoubleHZB,
        ForwardPlus_DoubleHZB_Passes, 6,
        false, true,  // requires_compute_shader = true
        GBufferQualityPreset::High,
        GBufferQualityPreset::Ultra,
        { 1, 1, true, false },
        // 高阶路径：固定 SSBO，避免双路径维护成本
        { VertexInputMigrationStage::SSBOOnly, false, false, true, PipelineInputMode::SSBOVertexInput }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // Mobile_Forward — 移动端简化前向
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Mobile_Forward_Passes[] = {
        { RenderStage::Forward_Opaque,      true,  true,  GBufferChannel::None, GBufferChannel::Color, PipelineCoverageMode::Solid, true },
        { RenderStage::Forward_Transparent, true,  false, GBufferChannel::None, GBufferChannel::Color, PipelineCoverageMode::Solid, true },
    };

    inline const RenderFlowDefinition Mobile_Forward = {
        "Mobile_Forward",
        RenderFlowPreset::Mobile_Forward,
        Mobile_Forward_Passes, 2,
        true, false,  // mobile_optimized = true
        GBufferQualityPreset::Low,
        GBufferQualityPreset::Medium,
        { 1, 1, false, false },
        // 轻量流：默认传统路径，必要时可切到 SSBO
        { VertexInputMigrationStage::DualPathValidation, true, true, true, PipelineInputMode::AutoPreferLegacy }
    };

    // ─── 流程索引表 ───
    inline const RenderFlowDefinition* const AllFlows[] = {
        &Forward_Basic,
        &Forward_WithEarlyZ,
        &Deferred_Standard,
        &ForwardPlus_DoubleHZB,
        &Mobile_Forward,
    };

    inline constexpr uint32_t FlowCount = sizeof(AllFlows) / sizeof(AllFlows[0]);

    // ─── 查找辅助函数 ───
    inline const RenderFlowDefinition* GetFlow(RenderFlowPreset preset) {
        for (uint32_t i = 0; i < FlowCount; ++i) {
            if (AllFlows[i]->preset == preset)
                return AllFlows[i];
        }
        return nullptr;
    }
}

}  // namespace hgl::graph::mtl
