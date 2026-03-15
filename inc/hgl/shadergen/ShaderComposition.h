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
#include <hgl/common/RenderFlowDef.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/type/String.h>
#include <string>
#include <vector>

namespace hgl::graph::mtl {

struct MaterialLogicDef;

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

    /// 自定义输出（由特定材质自行决定 RT count）
    Custom,

    ENUM_CLASS_RANGE(SingleRTAlphaBlend, Custom)
};

// 渲染流程/管线/覆盖模式等"渲染器 + ShaderGen 共用定义"已迁移到
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
// 辅助函数库生成策略：自动生成开发者使用的工具函数
// ─────────────────────────────────────────────────────────────────────────────

// ═════════════════════════════════════════════════════════════════════════════
// 预定义配置（编译期常量）
// ═════════════════════════════════════════════════════════════════════════════

// [已删除 GBufferConfigurations — GBuffer 系统已移除，使用 Forward/VBuffer 路径。]
/**
 * RenderFlows — 渲染流程预设表
 *
 * ShaderGen 遍历此表生成完整的 Shader 变体矩阵。
 */
namespace RenderFlows {
    // ═════════════════════════════════════════════════════════════════════════
    // Forward_Basic — 最基础的前向渲染
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Forward_Basic_Passes[] = {
        { RenderStage::Forward_Opaque,      true,  true,  RenderChannel::None, RenderChannel::Color, PipelineCoverageMode::Solid, true },
        { RenderStage::Forward_Masked,      true,  true,  RenderChannel::None, RenderChannel::Color, PipelineCoverageMode::Mask,  true },
        { RenderStage::Forward_Transparent, true,  false, RenderChannel::None, RenderChannel::Color, PipelineCoverageMode::Solid, true },
    };

    inline const RenderFlowDefinition Forward_Basic = {
        "Forward_Basic",
        RenderFlowPreset::Forward_Basic,
        Forward_Basic_Passes, 3,
        false, false,
        QualityTier::Low,
        QualityTier::Ultra,
        { 1, 1, false, false },
        // 迁移初期：保留双路径，默认偏向传统输入
        { VertexInputMigrationStage::DualPathValidation, true, true, true, PipelineInputMode::AutoPreferLegacy }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // Forward_WithEarlyZ — 带预深度的前向渲染
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Forward_WithEarlyZ_Passes[] = {
        { RenderStage::EarlyZ_Solid,        true,  true,  RenderChannel::None,  RenderChannel::Depth, PipelineCoverageMode::Solid, true  },
        { RenderStage::EarlyZ_Masked,       true,  true,  RenderChannel::None,  RenderChannel::Depth, PipelineCoverageMode::Mask,  false },
        { RenderStage::Forward_Opaque,      true,  false, RenderChannel::Depth, RenderChannel::Color, PipelineCoverageMode::Solid, true  },
        { RenderStage::Forward_Masked,      true,  false, RenderChannel::Depth, RenderChannel::Color, PipelineCoverageMode::Mask,  true  },
        { RenderStage::Forward_Transparent, true,  false, RenderChannel::Depth, RenderChannel::Color, PipelineCoverageMode::Solid, true  },
    };

    inline const RenderFlowDefinition Forward_WithEarlyZ = {
        "Forward_WithEarlyZ",
        RenderFlowPreset::Forward_WithEarlyZ,
        Forward_WithEarlyZ_Passes, 5,
        false, false,
        QualityTier::Low,
        QualityTier::Ultra,
        { 1, 1, false, false },
        // 迁移中期：默认优先 SSBO，保留回退
        { VertexInputMigrationStage::PreferSSBO, true, true, true, PipelineInputMode::AutoPreferSSBO }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // ForwardPlus_DoubleHZB — 双重 HZB 优化的 Forward+
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition ForwardPlus_DoubleHZB_Passes[] = {
        { RenderStage::EarlyZ_Solid,        true,  true,  RenderChannel::None,  RenderChannel::Depth, PipelineCoverageMode::Solid, true  },
        { RenderStage::HZB_Generation,      true,  false, RenderChannel::Depth, RenderChannel::None,  PipelineCoverageMode::Solid, true  },
        { RenderStage::HZB_Culling,         false, false, RenderChannel::None,  RenderChannel::None,  PipelineCoverageMode::Solid, true  },
        { RenderStage::Forward_Opaque,      true,  false, RenderChannel::Depth, RenderChannel::Color, PipelineCoverageMode::Solid, true  },
        { RenderStage::HZB_Generation,      true,  false, RenderChannel::Depth, RenderChannel::None,  PipelineCoverageMode::Solid, true  },
        { RenderStage::Forward_Transparent, true,  false, RenderChannel::Depth, RenderChannel::Color, PipelineCoverageMode::Solid, true  },
    };

    inline const RenderFlowDefinition ForwardPlus_DoubleHZB = {
        "ForwardPlus_DoubleHZB",
        RenderFlowPreset::ForwardPlus_DoubleHZB,
        ForwardPlus_DoubleHZB_Passes, 6,
        false, true,  // requires_compute_shader = true
        QualityTier::High,
        QualityTier::Ultra,
        { 1, 1, true, false },
        // 高阶路径：固定 SSBO，避免双路径维护成本
        { VertexInputMigrationStage::SSBOOnly, false, false, true, PipelineInputMode::SSBOVertexInput }
    };

    // ═════════════════════════════════════════════════════════════════════════
    // Mobile_Forward — 移动端简化前向
    // ═════════════════════════════════════════════════════════════════════════

    inline const RenderPassDefinition Mobile_Forward_Passes[] = {
        { RenderStage::Forward_Opaque,      true,  true,  RenderChannel::None, RenderChannel::Color, PipelineCoverageMode::Solid, true },
        { RenderStage::Forward_Transparent, true,  false, RenderChannel::None, RenderChannel::Color, PipelineCoverageMode::Solid, true },
    };

    inline const RenderFlowDefinition Mobile_Forward = {
        "Mobile_Forward",
        RenderFlowPreset::Mobile_Forward,
        Mobile_Forward_Passes, 2,
        true, false,  // mobile_optimized = true
        QualityTier::Low,
        QualityTier::Medium,
        { 1, 1, false, false },
        // 轻量流：默认传统路径，必要时可切到 SSBO
        { VertexInputMigrationStage::DualPathValidation, true, true, true, PipelineInputMode::AutoPreferLegacy }
    };

    // ─── 流程索引表 ───
    inline const RenderFlowDefinition* const AllFlows[] = {
        &Forward_Basic,
        &Forward_WithEarlyZ,
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
