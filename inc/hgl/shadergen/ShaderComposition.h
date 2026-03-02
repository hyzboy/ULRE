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

#include <hgl/shadergen/FixedMaterialDef.h>
#include <hgl/type/String.h>
#include <string>
#include <vector>

namespace hgl::graph::mtl {

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
    VertexInput = 0,
    SSBOVertexInput,
    AutoByCapability,

    ENUM_CLASS_RANGE(VertexInput, AutoByCapability)
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

// ─────────────────────────────────────────────────────────────────────────────
// 框架 Composer
// ─────────────────────────────────────────────────────────────────────────────

class ComposedShaderGenerator {
public:
    /**
     * 从合成定义生成完整 GLSL
     *
     * 内部流程：
     *   1. 构建前置（宏、引入）
     *   2. 定义通用结构体（VertexInput, VS_Output 等）
     *   3. 生成辅助函数库（GetLocalToWorld, GetNormal, GetMI 等）← 关键！
     *   4. 插入业务片段（开发者代码）
     *   5. 根据 output_mode 生成输出合成代码
     *   6. 根据 lighting_enabled + key 生成光照代码
     *   7. 合成 main() 和各个 shader entry point
     *
     * @return 完整 GLSL 源码
     */
    static std::string ComposeVertexShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const bool include_preamble = true);

    static std::string ComposeVertexShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const PipelineMode &pipeline_mode,
        const bool include_preamble = true);

    static ShaderComposeResult ComposeVertexShaderWithDiagnostics(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const PipelineMode &pipeline_mode,
        const bool include_preamble = true);

    static std::string ComposeFragmentShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const bool include_preamble = true);

    static std::string ComposeFragmentShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const PipelineMode &pipeline_mode,
        const bool include_preamble = true);

    static ShaderComposeResult ComposeFragmentShaderWithDiagnostics(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const PipelineMode &pipeline_mode,
        const bool include_preamble = true);

    static std::string ComposeGeometryShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const bool include_preamble = true);

    static std::string ComposeMeshShader(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const PipelineMode &pipeline_mode,
        const bool include_preamble = true);

private:
    // ─────────────────────────────────────────────────────────────────────────
    // 结构体和常数定义生成
    // ─────────────────────────────────────────────────────────────────────────
    static std::string GenVertexInputStruct(const ComposedMaterialDef &def);
    static std::string GenVSOutputStruct(const ComposedMaterialDef &def);
    static std::string GenLightingOutputStruct();

    // ─────────────────────────────────────────────────────────────────────────
    // 辅助函数库生成（这是关键部分！）
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * 根据 ComposedMaterialDef 和 Shader Stage 生成辅助函数库
     * 
     * 生成的函数包括：
     *   - GetLocalToWorld() 及相关矩阵变换
     *   - GetNormal() / GetNormalMatrix()
     *   - GetPosition3D() / GetClipPosition() 等位置相关函数
     *   - GetMaterialInstance() / GetMI()
     *   - HandoverMaterialInstanceID() [仅 GS 需要]
     *
     * @param def 材质定义（描述符、顶点输入等）
     * @param shader_stage VS/GS/FS 标记，影响函数签名
     * @return 完整的 GLSL 函数库代码
     */
    static std::string GenHelperFunctionLibrary(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key,
        const char *shader_stage);

    // 具体的函数生成方法
    static std::string GenGetLocalToWorld(const ComposedMaterialDef &def);
    static std::string GenGetNormalMatrix(const ComposedMaterialDef &def);
    static std::string GenGetNormalFunction(const ComposedMaterialDef &def, const char *shader_stage);
    static std::string GenGetPositionFunctions(const ComposedMaterialDef &def, const char *shader_stage);
    static std::string GenGetMaterialInstanceFunctions(const ComposedMaterialDef &def, const char *shader_stage);

    // ─────────────────────────────────────────────────────────────────────────
    // 其他生成方法
    // ─────────────────────────────────────────────────────────────────────────
    static std::string GenCoordinateTransformFunctions();
    static std::string GenOutputCompositionCode(ShaderOutputMode mode);
    static std::string GenLightingCode(
        const ComposedMaterialDef &def,
        const ShaderPermutationKey &key);
};

}  // namespace hgl::graph::mtl
