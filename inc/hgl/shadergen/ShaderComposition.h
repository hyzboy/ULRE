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

enum class PipelineRenderPath : uint8 {
    Forward = 0,
    GBufferDeferred,
    VBufferDeferred,
    MobileSubpassGBufferDeferred,
    PostProcess,

    ENUM_CLASS_RANGE(Forward, PostProcess)
};

enum class PipelineCoverageMode : uint8 {
    // 常规颜色渲染覆盖模式
    Solid = 0,
    Mask,

    // 纯深度渲染覆盖模式（ShadowMap / Early-Z 等）
    DepthOnlySolid,
    DepthOnlyMask,

    ENUM_CLASS_RANGE(Solid, DepthOnlyMask)
};

enum class PipelineInputMode : uint8 {
    // 传统顶点输入：VAB/VBO (Vertex Attribute Binding + Vertex Buffer Object)
    LegacyVABVBO = 0,

    // 兼容旧命名（等价于 LegacyVABVBO）
    VertexInput = LegacyVABVBO,

    // 统一从 SSBO 读取顶点数据
    SSBOVertexInput,

    // 同时支持传统 VAB/VBO 与 SSBO 双路径（迁移期常用）
    HybridVABVBOAndSSBO,

    // 自动选择，但优先 SSBO
    AutoPreferSSBO,

    // 自动选择，但优先传统 VAB/VBO
    AutoPreferLegacy,

    // 运行时按能力策略自动选择
    AutoByCapability,

    ENUM_CLASS_RANGE(LegacyVABVBO, AutoByCapability)
};

/**
 * VertexInputMigrationStage — 顶点输入迁移阶段
 *
 * 用于表达项目从传统 VAB/VBO 向 SSBO 迁移的当前阶段，
 * 方便 ShaderGen 与运行时共享同一语义。
 */
enum class VertexInputMigrationStage : uint8 {
    LegacyOnly = 0,      ///< 仅传统 VAB/VBO 路径
    DualPathValidation,  ///< 双路径并存，进行一致性验证
    PreferSSBO,          ///< 默认 SSBO，必要时回退传统路径
    SSBOOnly,            ///< 仅 SSBO 路径

    ENUM_CLASS_RANGE(LegacyOnly, SSBOOnly)
};

/**
 * VertexInputMigrationPolicy — 顶点输入迁移策略
 *
 * 该结构用于定义整个渲染流程在顶点输入上的迁移策略：
 * - ShaderGen 是否生成双路径变体
 * - 运行时是否允许回退
 * - Flow 内是否强制统一输入模式
 */
struct VertexInputMigrationPolicy {
    VertexInputMigrationStage stage = VertexInputMigrationStage::PreferSSBO;

    // 是否允许从 SSBO 回退到传统 VAB/VBO（仅在非 SSBOOnly 阶段有效）
    bool allow_legacy_fallback = true;

    // 迁移期间是否同时保留双路径 Shader 变体
    bool keep_dual_path_shader_variants = true;

    // 是否要求同一 RenderFlow 内所有 Pass 使用同一输入模式
    bool require_consistent_mode_in_flow = true;

    // 当 Pass 未显式覆盖时使用的默认输入模式
    PipelineInputMode default_input_mode = PipelineInputMode::AutoPreferSSBO;
};

enum class PipelineTopology : uint8 {
    VSFS = 0,
    MeshFS,
    AutoByCapability,

    ENUM_CLASS_RANGE(VSFS, AutoByCapability)
};

enum class GBufferChannel : uint32_t {
    None          = 0,
    Color         = 1u << 0,
    Normal        = 1u << 1,
    Depth         = 1u << 2,
    Emissive      = 1u << 3,
    MotionVector  = 1u << 4,
    Specular      = 1u << 5,
    Roughness     = 1u << 6,
    Metallic      = 1u << 7,
    AO            = 1u << 8,
};

inline GBufferChannel operator|(GBufferChannel a, GBufferChannel b)
{
    return GBufferChannel(uint32_t(a) | uint32_t(b));
}

inline GBufferChannel operator&(GBufferChannel a, GBufferChannel b)
{
    return GBufferChannel(uint32_t(a) & uint32_t(b));
}

inline GBufferChannel &operator|=(GBufferChannel &a, GBufferChannel b)
{
    a = a | b;
    return a;
}

enum class GBufferFormatLevel : uint8 {
    // 手机极低配：Color + Normal + Depth
    MobileLite = 0,

    // 手机/主机中档：在 MobileLite 基础上增加 Emissive（MotionVector 按需开启）
    MobileExtended,

    // 桌面标准：增加 PBR 关键通道
    DesktopStandard,

    // 桌面高配：完整延迟通道集
    DesktopFull,

    // 自定义通道集合
    Custom,

    ENUM_CLASS_RANGE(MobileLite, Custom)
};

struct GBufferFormatSpec {
    // 格式级别（与光照模式档位协同）
    GBufferFormatLevel level = GBufferFormatLevel::MobileLite;

    // 该格式可输出的通道掩码
    GBufferChannel channel_mask = GBufferChannel::None;

    // 自动生成到 shader 的参数结构体名称
    const char *generated_param_struct_name = "GBufferParams";

    // true: 生成器自动生成上述参数结构体
    bool auto_generate_param_struct = true;

    // MotionVector 是否加入该格式（默认关闭，尤其是移动端）
    bool enable_motion_vector = false;
};

enum class PipelineForwardLightingMode : uint8 {
    // 默认：片元光照（质量优先）
    PerPixel = 0,

    // 顶点光照（极低配/远景对象）
    PerVertex,

    // 运行时按能力与距离策略自动选择
    AutoByCapability,

    ENUM_CLASS_RANGE(PerPixel, AutoByCapability)
};

enum class NormalEncodingMode : uint8 {
    None = 0,
    Octahedral,
    Spheremap,

    ENUM_CLASS_RANGE(None, Spheremap)
};

struct NormalCompressionPolicy {
    // 顶点输入 normal：存储压缩 + 读取自动解压
    bool compress_vertex_input_normal = false;
    NormalEncodingMode vertex_input_encoding = NormalEncodingMode::Octahedral;

    // 法线贴图 normal：采样后自动解压到线性法线
    bool compress_normal_map = false;
    NormalEncodingMode normal_map_encoding = NormalEncodingMode::Octahedral;

    // GBuffer normal：写入自动压缩，读取自动解压
    bool compress_gbuffer_normal = false;
    NormalEncodingMode gbuffer_encoding = NormalEncodingMode::Octahedral;
};

struct PipelineMode {
    PipelineRenderPath render_path = PipelineRenderPath::Forward;
    PipelineCoverageMode coverage = PipelineCoverageMode::Solid;
    PipelineInputMode input_mode = PipelineInputMode::AutoByCapability;
    PipelineTopology topology = PipelineTopology::AutoByCapability;

    // 延迟渲染输出格式（GBuffer/VBuffer 必填；Forward/PostProcess 可复用）
    GBufferFormatSpec gbuffer_format;

    // 后处理输出通道组合（必须是 gbuffer_format.channel_mask 的子集）
    // 为 None 时表示自动继承 gbuffer_format.channel_mask
    GBufferChannel postprocess_output_channels = GBufferChannel::None;

    PipelineForwardLightingMode forward_lighting = PipelineForwardLightingMode::PerPixel;

    // 法线压缩策略（生成器自动插入编码/解码 helper）
    NormalCompressionPolicy normal_compression;
};

// ═════════════════════════════════════════════════════════════════════════════
// 渲染流程三层架构：语义层 → 配置层 → 编排层
// ═════════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
// Layer 1: 渲染阶段（语义层）- "做什么"
// ─────────────────────────────────────────────────────────────────────────────

/**
 * RenderStage — 渲染阶段语义定义
 *
 * 定义了渲染流程中的原子操作单元，每个枚举值代表一个明确的渲染目的。
 * ShaderGen 根据 Stage 类型生成不同的 Shader 代码。
 */
enum class RenderStage : uint8 {
    // ─── 深度预通道 ───
    EarlyZ_Solid = 0,           ///< Early-Z 实体深度预渲染
    EarlyZ_Masked,              ///< Early-Z 带 Alpha Test 的深度预渲染

    // ─── 阴影贴图 ───
    ShadowMap_Directional,      ///< 方向光阴影贴图（Cascade）
    ShadowMap_Spot,             ///< 聚光灯阴影贴图
    ShadowMap_Point,            ///< 点光源阴影贴图（CubeMap）

    // ─── GBuffer 填充 ───
    GBuffer_Opaque,             ///< GBuffer 不透明物体填充
    GBuffer_Masked,             ///< GBuffer 带 Alpha Test 的物体填充

    // ─── 可见性 Buffer（现代渲染技术）───
    VisibilityBuffer_Fill,      ///< Visibility Buffer 填充（只写三角形 ID + 深度）

    // ─── 延迟光照 ───
    Deferred_Lighting,          ///< 标准延迟光照
    Deferred_LightingTiled,     ///< 分块延迟光照（Tiled Deferred）
    Deferred_LightingClustered, ///< 分簇延迟光照（Clustered Deferred）

    // ─── 前向渲染 ───
    Forward_Opaque,             ///< 前向渲染不透明物体
    Forward_Masked,             ///< 前向渲染带 Alpha Test 的物体
    Forward_Transparent,        ///< 前向渲染半透明物体（Alpha Blend）
    Forward_Additive,           ///< 前向渲染加色混合（光效、爆炸）

    // ─── 优化技术 ───
    HZB_Generation,             ///< 生成层级深度缓冲（Hierarchical Z-Buffer）
    HZB_Culling,                ///< 基于 HZB 的光源/物体剔除

    // ─── 后处理 ───
    PostProcess_TAA,            ///< 时序抗锯齿（Temporal Anti-Aliasing）
    PostProcess_Bloom,          ///< 泛光效果
    PostProcess_ToneMapping,    ///< 色调映射（HDR → LDR）
    PostProcess_FXAA,           ///< 快速近似抗锯齿
    PostProcess_MotionBlur,     ///< 运动模糊
    PostProcess_DOF,            ///< 景深（Depth of Field）
    PostProcess_SSR,            ///< 屏幕空间反射
    PostProcess_SSAO,           ///< 屏幕空间环境光遮蔽

    // ─── 调试可视化 ───
    Debug_Visualization,        ///< 调试可视化（GBuffer 通道查看、线框等）

    ENUM_CLASS_RANGE(EarlyZ_Solid, Debug_Visualization)
};

// ─────────────────────────────────────────────────────────────────────────────
// GBuffer 画质配置（正交于渲染流程的维度）
// ─────────────────────────────────────────────────────────────────────────────

/**
 * GBufferQualityPreset — GBuffer 画质档位预设
 *
 * 在游戏启动时根据硬件能力或玩家设置选择一个档位，运行时不再改变。
 * ShaderGen 为每个档位生成不同的 Shader 变体。
 */
enum class GBufferQualityPreset : uint8 {
    // ─── 平台无关档位（按画质与资源预算递增）───
    Low = 0,             ///< 低档：Color + Normal + Depth
    LowPlus,             ///< 低档增强：+ Emissive
    Medium,              ///< 中档：+ MotionVector
    MediumPlus,          ///< 中高档：+ Specular
    High,                ///< 高档：+ Roughness
    HighPlus,            ///< 高档增强：+ Metallic
    Ultra,               ///< 旗舰档：+ AO

    ENUM_CLASS_RANGE(Low, Ultra)
};

/**
 * GBufferConfiguration — GBuffer 配置（编译期预设）
 *
 * 每个画质档位对应一套完整的 GBuffer 配置。
 * ShaderGen 根据此配置生成对应的 Shader 代码（输入/输出结构体、编解码逻辑）。
 */
struct GBufferConfiguration {
    GBufferQualityPreset preset;
    GBufferFormatLevel level;
    GBufferChannel channel_mask;
    bool enable_motion_vector;
    NormalCompressionPolicy normal_compression;

    /// 由配置字段自动计算得到的变体哈希（用于区分 Shader 变体）
    uint32_t variant_hash;

    // 重新计算当前配置的变体哈希（用于校验）
    uint32_t RecomputeVariantHash() const;
};

// FNV-1a 32-bit 轻量哈希，用于稳定生成 GBuffer 变体键
constexpr uint32_t HashFNV1a32(uint32_t seed, uint32_t value)
{
    return (seed ^ value) * 16777619u;
}

// 根据 GBuffer 相关配置字段计算变体哈希
constexpr uint32_t ComputeGBufferVariantHash(
    GBufferQualityPreset preset,
    GBufferFormatLevel level,
    GBufferChannel channel_mask,
    bool enable_motion_vector,
    const NormalCompressionPolicy &normal_compression)
{
    uint32_t h = 2166136261u;  // FNV-1a offset basis

    h = HashFNV1a32(h, uint32_t(preset));
    h = HashFNV1a32(h, uint32_t(level));
    h = HashFNV1a32(h, uint32_t(channel_mask));
    h = HashFNV1a32(h, enable_motion_vector ? 1u : 0u);

    h = HashFNV1a32(h, normal_compression.compress_vertex_input_normal ? 1u : 0u);
    h = HashFNV1a32(h, uint32_t(normal_compression.vertex_input_encoding));

    h = HashFNV1a32(h, normal_compression.compress_normal_map ? 1u : 0u);
    h = HashFNV1a32(h, uint32_t(normal_compression.normal_map_encoding));

    h = HashFNV1a32(h, normal_compression.compress_gbuffer_normal ? 1u : 0u);
    h = HashFNV1a32(h, uint32_t(normal_compression.gbuffer_encoding));

    return h;
}

inline uint32_t GBufferConfiguration::RecomputeVariantHash() const
{
    return ComputeGBufferVariantHash(
        preset,
        level,
        channel_mask,
        enable_motion_vector,
        normal_compression);
}

// ─────────────────────────────────────────────────────────────────────────────
// Layer 2: Pass 定义（配置层）- "怎么做"
// ─────────────────────────────────────────────────────────────────────────────

/**
 * RenderPassDefinition — 单个 Pass 的完整定义
 *
 * 将 Layer 1 的语义（RenderStage）与具体的渲染状态配置结合。
 * ShaderGen 根据此定义生成对应的 Shader 代码（输入/输出、深度状态等）。
 */
struct RenderPassDefinition {
    RenderStage stage;              ///< 语义层引用（做什么）

    // ─── 深度/模板状态（编译期固定，影响 Pipeline State）───
    bool depth_test;
    bool depth_write;

    // ─── 输入输出（编译期固定，ShaderGen 据此生成代码）───
    GBufferChannel read_channels;   ///< 该 Pass 读取哪些 GBuffer 通道
    GBufferChannel write_channels;  ///< 该 Pass 写入哪些 GBuffer 通道

    // ─── 材质过滤 ───
    PipelineCoverageMode coverage_mode;  ///< 该 Pass 渲染哪种覆盖模式的材质

    // ─── 执行控制 ───
    bool mandatory;  ///< true = 必须执行，false = 可通过配置禁用

    // ─── 顶点输入模式（可选覆盖）───
    // 当为 AutoByCapability 时，使用 RenderFlowDefinition.vertex_input_policy.default_input_mode
    PipelineInputMode input_mode = PipelineInputMode::AutoByCapability;
};

// ─────────────────────────────────────────────────────────────────────────────
// Layer 3: Flow 定义（编排层）- "什么顺序"
// ─────────────────────────────────────────────────────────────────────────────

/**
 * RenderFlowPreset — 渲染流程预设枚举
 *
 * 预定义的完整渲染流程类型，每种对应一套 Pass 序列。
 */
enum class RenderFlowPreset : uint8 {
    // ─── 基础前向渲染 ───
    Forward_Basic = 0,           ///< Opaque + Masked + Transparent
    Forward_WithEarlyZ,          ///< EarlyZ + Opaque + Transparent

    // ─── Forward+ 系列 ───
    ForwardPlus_SingleHZB,       ///< EarlyZ + HZB + Opaque + Transparent
    ForwardPlus_DoubleHZB,       ///< EarlyZ + HZB + Culling + HZB2 + Opaque + Transparent

    // ─── 延迟渲染 ───
    Deferred_Standard,           ///< GBuffer + Lighting + Transparent
    Deferred_Tiled,              ///< GBuffer + TiledLighting + Transparent
    Deferred_Clustered,          ///< GBuffer + ClusteredLighting + Transparent

    // ─── 现代技术 ───
    VisibilityBuffer_Deferred,   ///< Visibility Buffer + 延迟着色

    // ─── 移动端优化 ───
    Mobile_Forward,              ///< 移动端简化前向（无 EarlyZ）
    Mobile_SubpassDeferred,      ///< 移动端 Subpass 优化的延迟

    ENUM_CLASS_RANGE(Forward_Basic, Mobile_SubpassDeferred)
};

/**
 * RenderFlowDefinition — 渲染流程定义（编译期静态）
 *
 * 定义了一个完整的渲染流程：Pass 序列、执行顺序、平台特性、资源需求。
 * ShaderGen 遍历所有 Flow，为每个 Pass 生成对应的 Shader 变体。
 */
struct RenderFlowDefinition {
    const char *name;                       ///< 流程名称（用于 SPV 命名）
    RenderFlowPreset preset;                ///< 流程类型枚举

    // ─── Pass 序列（按执行顺序排列）───
    const RenderPassDefinition *passes;     ///< Pass 定义数组（编译期常量）
    uint32_t pass_count;                    ///< Pass 数量

    // ─── 平台特性 ───
    bool mobile_optimized;                  ///< 是否为移动端优化
    bool requires_compute_shader;           ///< 是否需要计算着色器（如 HZB）

    // ─── 支持的画质范围（用于验证配置有效性）───
    GBufferQualityPreset min_required_quality;   ///< 最低画质要求
    GBufferQualityPreset max_supported_quality;  ///< 最高支持画质

    // ─── 资源需求（编译期常量，用于验证和优化）───
    struct ResourceRequirement {
        uint32_t color_attachment_count;    ///< 需要的颜色附件数量
        uint32_t depth_attachment_count;    ///< 需要的深度附件数量
        bool need_hzb_texture;              ///< 是否需要 HZB 纹理
        bool need_shadow_map;               ///< 是否需要阴影贴图
    } resource_requirements;

    // ─── 顶点输入迁移策略（VAB/VBO -> SSBO）───
    VertexInputMigrationPolicy vertex_input_policy;
};

// ─────────────────────────────────────────────────────────────────────────────
// 运行时配置：Flow × GBuffer = 完整渲染管线
// ─────────────────────────────────────────────────────────────────────────────

/**
 * RenderPipeline — 运行时渲染管线配置
 *
 * 在游戏启动时根据硬件能力/玩家设置选择一个 Flow 和一个 GBuffer 配置，
 * 之后在整个运行期间不再改变。
 *
 * 用途：
 *   - ShaderGen：根据此配置生成 SPV 文件名
 *   - 运行时：根据此配置加载对应的 SPV 并执行渲染
 */
struct RenderPipeline {
    const RenderFlowDefinition *flow;           ///< 选定的渲染流程
    const GBufferConfiguration *gbuffer_config; ///< 选定的 GBuffer 配置

    // ─── 验证配置有效性 ───
    bool IsValid() const {
        return gbuffer_config->preset >= flow->min_required_quality &&
               gbuffer_config->preset <= flow->max_supported_quality;
    }

    // ─── 获取 SPV 文件路径（ShaderGen 命名规则）───
    /// 例如：Deferred_Standard_GBuffer_Opaque_9ABC0123.spv
    std::string GetSPVPath(RenderStage stage, const char* stage_suffix = nullptr) const;

    // ─── 获取配置哈希（用于快速比较和缓存键）───
    uint64_t GetConfigHash() const {
        return (uint64_t(flow->preset) << 32) | gbuffer_config->variant_hash;
    }
};

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

// ═════════════════════════════════════════════════════════════════════════════
// RenderPipeline 实现
// ═════════════════════════════════════════════════════════════════════════════

inline std::string RenderPipeline::GetSPVPath(RenderStage stage, const char* stage_suffix) const {
    // SPV 命名规则：FlowName_StageName_VariantHash.spv
    // 例如：Deferred_Standard_GBuffer_Opaque_5E6F7081.spv

    const char* stage_name = [stage]() -> const char* {
        switch (stage) {
            case RenderStage::EarlyZ_Solid:           return "EarlyZ_Solid";
            case RenderStage::EarlyZ_Masked:          return "EarlyZ_Masked";
            case RenderStage::ShadowMap_Directional:  return "ShadowMap_Directional";
            case RenderStage::ShadowMap_Spot:         return "ShadowMap_Spot";
            case RenderStage::ShadowMap_Point:        return "ShadowMap_Point";
            case RenderStage::GBuffer_Opaque:         return "GBuffer_Opaque";
            case RenderStage::GBuffer_Masked:         return "GBuffer_Masked";
            case RenderStage::VisibilityBuffer_Fill:  return "VisibilityBuffer_Fill";
            case RenderStage::Deferred_Lighting:      return "Deferred_Lighting";
            case RenderStage::Deferred_LightingTiled: return "Deferred_LightingTiled";
            case RenderStage::Deferred_LightingClustered: return "Deferred_LightingClustered";
            case RenderStage::Forward_Opaque:         return "Forward_Opaque";
            case RenderStage::Forward_Masked:         return "Forward_Masked";
            case RenderStage::Forward_Transparent:    return "Forward_Transparent";
            case RenderStage::Forward_Additive:       return "Forward_Additive";
            case RenderStage::HZB_Generation:         return "HZB_Generation";
            case RenderStage::HZB_Culling:            return "HZB_Culling";
            case RenderStage::PostProcess_TAA:        return "PostProcess_TAA";
            case RenderStage::PostProcess_Bloom:      return "PostProcess_Bloom";
            case RenderStage::PostProcess_ToneMapping: return "PostProcess_ToneMapping";
            case RenderStage::PostProcess_FXAA:       return "PostProcess_FXAA";
            case RenderStage::PostProcess_MotionBlur: return "PostProcess_MotionBlur";
            case RenderStage::PostProcess_DOF:        return "PostProcess_DOF";
            case RenderStage::PostProcess_SSR:        return "PostProcess_SSR";
            case RenderStage::PostProcess_SSAO:       return "PostProcess_SSAO";
            case RenderStage::Debug_Visualization:    return "Debug_Visualization";
            default: return "Unknown";
        }
    }();

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s_%s%s_%08X.spv",
             flow->name,
             stage_name,
             stage_suffix ? stage_suffix : "",
             gbuffer_config->variant_hash);

    return std::string(buffer);
}

}  // namespace hgl::graph::mtl
