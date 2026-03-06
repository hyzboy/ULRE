/// ComposedShaderGenerator.cpp — 合成型着色器生成器实现
///
/// 本文件实现 ComposedShaderGenerator，根据 ComposedMaterialDef + ShaderPermutationKey
/// 自动生成完整的 GLSL 着色器代码，包括：
/// - 前置宏和版本声明
/// - 通用结构体定义
/// - 辅助函数库（坐标变换、法线、材质实例等）
/// - 开发者业务代码
/// - 光照计算（根据 permutation key）
/// - 输出合成（根据 output mode）
/// - Main 函数组装

#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderLogic.h>
#include <hgl/shadergen/ResourceLayoutGenerator.h>
#include <hgl/shadergen/BuiltinHelpers.h>
#include <hgl/mtl/StdMaterial.h>
#include "common/MFSkyLight.h"  // SKYLIGHT_GLSL_HEADER, GetSkyLightModelImplGLSL
#include <string>

namespace hgl::graph::mtl {

static bool CStrEq(const char *lhs, const char *rhs)
{
    return lhs && rhs && std::strcmp(lhs, rhs) == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数：顶点属性检查
// ─────────────────────────────────────────────────────────────────────────────

static bool HasVertexAttribute(
    const ComposedMaterialDef &def,
    const char *name)
{
    if (!name)
        return false;

    for (uint32_t i = 0; i < def.vertex_entry_count; i++) {
        const auto &entry = def.vertex_entries[i];
        if (CStrEq(entry.name, name)) {
            return true;
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数：描述符查找
// ─────────────────────────────────────────────────────────────────────────────

static const FixedDescriptorEntry *FindDescriptorByName(
    const ComposedMaterialDef &def,
    const char *name)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; i++) {
        const auto &entry = def.descriptor_entries[i];
        if (CStrEq(entry.name, name)
         || CStrEq(entry.struct_name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

static PipelineMode ResolvePipelineModeForCurrentBackend(const PipelineMode &requested)
{
    PipelineMode resolved = requested;

    auto GetDefaultGBufferChannelMask = [](const GBufferFormatLevel level, const bool enable_motion_vector) -> GBufferChannel
    {
        GBufferChannel mask = GBufferChannel::None;

        switch (level)
        {
            case GBufferFormatLevel::MobileLite:
                mask = GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth;
                break;

            case GBufferFormatLevel::MobileExtended:
                mask = GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth
                     | GBufferChannel::Emissive;
                break;

            case GBufferFormatLevel::DesktopStandard:
                mask = GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth
                     | GBufferChannel::Emissive
                     | GBufferChannel::Roughness | GBufferChannel::Metallic;
                break;

            case GBufferFormatLevel::DesktopFull:
                mask = GBufferChannel::Color | GBufferChannel::Normal | GBufferChannel::Depth
                     | GBufferChannel::Emissive
                     | GBufferChannel::Specular | GBufferChannel::Roughness
                     | GBufferChannel::Metallic | GBufferChannel::AO;
                break;

            case GBufferFormatLevel::Custom:
            default:
                mask = GBufferChannel::None;
                break;
        }

        if (enable_motion_vector)
            mask |= GBufferChannel::MotionVector;

        return mask;
    };

    // SG-1: 仅建立模式轴与路由骨架。
    // 当前后端仍使用 VS/FS + VertexInput 作为稳定实现。
    if (resolved.input_mode == PipelineInputMode::AutoByCapability)
        resolved.input_mode = PipelineInputMode::VertexInput;

    if (resolved.topology == PipelineTopology::AutoByCapability)
        resolved.topology = PipelineTopology::VSFS;

    if (resolved.forward_lighting == PipelineForwardLightingMode::AutoByCapability)
        resolved.forward_lighting = PipelineForwardLightingMode::PerPixel;

    // 前向光照模式仅对 Forward 路径有效，其他路径回退到 PerPixel（占位语义）
    if (resolved.render_path != PipelineRenderPath::Forward)
        resolved.forward_lighting = PipelineForwardLightingMode::PerPixel;

    // GBuffer 格式默认通道
    if (resolved.gbuffer_format.channel_mask == GBufferChannel::None
     && resolved.gbuffer_format.level != GBufferFormatLevel::Custom)
    {
        resolved.gbuffer_format.channel_mask = GetDefaultGBufferChannelMask(
            resolved.gbuffer_format.level,
            resolved.gbuffer_format.enable_motion_vector);
    }

    // 后处理输出通道：默认继承 GBuffer 格式；并裁剪为其子集
    if (resolved.postprocess_output_channels == GBufferChannel::None)
    {
        resolved.postprocess_output_channels = resolved.gbuffer_format.channel_mask;
    }
    else
    {
        resolved.postprocess_output_channels =
            (resolved.postprocess_output_channels & resolved.gbuffer_format.channel_mask);
    }

    auto NormalizeNormalCompression = [](bool &compress, NormalEncodingMode &encoding)
    {
        if (!compress)
        {
            encoding = NormalEncodingMode::None;
            return;
        }

        if (encoding == NormalEncodingMode::None)
        {
            compress = false;
        }
    };

    NormalizeNormalCompression(
        resolved.normal_compression.compress_vertex_input_normal,
        resolved.normal_compression.vertex_input_encoding);

    NormalizeNormalCompression(
        resolved.normal_compression.compress_normal_map,
        resolved.normal_compression.normal_map_encoding);

    NormalizeNormalCompression(
        resolved.normal_compression.compress_gbuffer_normal,
        resolved.normal_compression.gbuffer_encoding);

    return resolved;
}

struct NormalCompressionNormalizationDiagnostics
{
    bool vertex_input_normalized = false;
    bool normal_map_normalized = false;
    bool gbuffer_normal_normalized = false;

    bool Any() const
    {
        return vertex_input_normalized || normal_map_normalized || gbuffer_normal_normalized;
    }
};

static NormalCompressionNormalizationDiagnostics BuildNormalCompressionNormalizationDiagnostics(
    const PipelineMode &requested,
    const PipelineMode &resolved)
{
    NormalCompressionNormalizationDiagnostics diagnostics;

    diagnostics.vertex_input_normalized =
        (requested.normal_compression.compress_vertex_input_normal != resolved.normal_compression.compress_vertex_input_normal)
     || (requested.normal_compression.vertex_input_encoding != resolved.normal_compression.vertex_input_encoding);

    diagnostics.normal_map_normalized =
        (requested.normal_compression.compress_normal_map != resolved.normal_compression.compress_normal_map)
     || (requested.normal_compression.normal_map_encoding != resolved.normal_compression.normal_map_encoding);

    diagnostics.gbuffer_normal_normalized =
        (requested.normal_compression.compress_gbuffer_normal != resolved.normal_compression.compress_gbuffer_normal)
     || (requested.normal_compression.gbuffer_encoding != resolved.normal_compression.gbuffer_encoding);

    return diagnostics;
}

static std::string GenNormalCompressionNormalizationComments(
    const NormalCompressionNormalizationDiagnostics &diagnostics)
{
    if (!diagnostics.Any())
        return "";

    std::string result;
    result += "// NORMAL_COMPRESSION_POLICY_NORMALIZED\n";
    result += diagnostics.vertex_input_normalized
           ? "// NORMAL_POLICY_NORMALIZED_VERTEX_INPUT=1\n"
           : "// NORMAL_POLICY_NORMALIZED_VERTEX_INPUT=0\n";
    result += diagnostics.normal_map_normalized
           ? "// NORMAL_POLICY_NORMALIZED_NORMAL_MAP=1\n"
           : "// NORMAL_POLICY_NORMALIZED_NORMAL_MAP=0\n";
    result += diagnostics.gbuffer_normal_normalized
           ? "// NORMAL_POLICY_NORMALIZED_GBUFFER=1\n"
           : "// NORMAL_POLICY_NORMALIZED_GBUFFER=0\n";
    result += "\n";

    return result;
}

static ShaderComposeDiagnostics ToPublicDiagnostics(
    const NormalCompressionNormalizationDiagnostics &diagnostics)
{
    ShaderComposeDiagnostics out;
    out.normal_compression_policy_normalized = diagnostics.Any();
    out.normal_policy_normalized_vertex_input = diagnostics.vertex_input_normalized;
    out.normal_policy_normalized_normal_map = diagnostics.normal_map_normalized;
    out.normal_policy_normalized_gbuffer = diagnostics.gbuffer_normal_normalized;
    return out;
}

static void CollectHelperConflictDiagnosticsFromCode(
    const std::string &code,
    ShaderComposeDiagnostics &diagnostics)
{
    diagnostics.helper_conflicts.clear();
    diagnostics.helper_conflict_count = 0;
    diagnostics.helper_conflict_detected = false;

    const char *marker = "// HELPER_CONFLICT:";
    const char *cursor = code.c_str();
    if (!cursor)
        return;

    while ((cursor = std::strstr(cursor, marker)) != nullptr)
    {
        cursor += std::strlen(marker);

        while (*cursor == ' ' || *cursor == '\t')
            ++cursor;

        const char *line_end = std::strchr(cursor, '\n');
        if (!line_end)
            line_end = cursor + std::strlen(cursor);

        if (line_end > cursor)
            diagnostics.helper_conflicts.emplace_back(cursor, size_t(line_end - cursor));

        diagnostics.helper_conflict_count++;
        diagnostics.helper_conflict_detected = true;
        cursor = line_end;
    }
}

static void AppendEncodingDefines(
    std::string &result,
    const char *prefix,
    const NormalEncodingMode mode)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "#define %s_NONE %d\n", prefix,
             mode == NormalEncodingMode::None ? 1 : 0);
    result += buf;

    snprintf(buf, sizeof(buf), "#define %s_OCT %d\n", prefix,
             mode == NormalEncodingMode::Octahedral ? 1 : 0);
    result += buf;

    snprintf(buf, sizeof(buf), "#define %s_SPHEREMAP %d\n", prefix,
             mode == NormalEncodingMode::Spheremap ? 1 : 0);
    result += buf;
}

static std::string GenNormalCompressionDefines(const PipelineMode &mode)
{
    std::string result;

    char buf[256];
    snprintf(buf, sizeof(buf), "#define COMPRESS_VERTEX_INPUT_NORMAL %d\n",
             mode.normal_compression.compress_vertex_input_normal ? 1 : 0);
    result += buf;
    snprintf(buf, sizeof(buf), "#define COMPRESS_NORMAL_MAP %d\n",
             mode.normal_compression.compress_normal_map ? 1 : 0);
    result += buf;
    snprintf(buf, sizeof(buf), "#define COMPRESS_GBUFFER_NORMAL %d\n",
             mode.normal_compression.compress_gbuffer_normal ? 1 : 0);
    result += buf;

    AppendEncodingDefines(result, "VERTEX_NORMAL_ENCODING", mode.normal_compression.vertex_input_encoding);
    AppendEncodingDefines(result, "NORMAL_MAP_ENCODING", mode.normal_compression.normal_map_encoding);
    AppendEncodingDefines(result, "GBUFFER_NORMAL_ENCODING", mode.normal_compression.gbuffer_encoding);

    result += "\n";
    return result;
}

static std::string GenNormalCompressionHelpers()
{
    return R"(
// Normal compression helpers (SG-2 template)
vec2 EncodeNormalOct(vec3 n) {
    n = normalize(n);
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 enc = n.xy;
    if (n.z < 0.0) {
        enc = (1.0 - abs(enc.yx)) * sign(enc.xy);
    }
    return enc * 0.5 + 0.5;
}

vec3 DecodeNormalOct(vec2 e) {
    vec2 f = e * 2.0 - 1.0;
    vec3 n = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = clamp(-n.z, 0.0, 1.0);
    n.xy += vec2(n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t);
    return normalize(n);
}

vec2 EncodeNormalSpheremap(vec3 n) {
    n = normalize(n);
    float m = sqrt(max(2.0 * n.z + 2.0, 1e-6));
    return n.xy / m + 0.5;
}

vec3 DecodeNormalSpheremap(vec2 e) {
    vec2 f = e * 4.0 - 2.0;
    float ff = dot(f, f);
    float g = sqrt(max(1.0 - ff * 0.25, 0.0));
    vec3 n;
    n.xy = f * g;
    n.z = 1.0 - ff * 0.5;
    return normalize(n);
}

vec3 DecodeVertexInputNormal(vec3 normal_in) {
#if COMPRESS_VERTEX_INPUT_NORMAL
#if VERTEX_NORMAL_ENCODING_SPHEREMAP
    return DecodeNormalSpheremap(normal_in.xy);
#elif VERTEX_NORMAL_ENCODING_OCT
    return DecodeNormalOct(normal_in.xy);
#else
    return normalize(normal_in);
#endif
#else
    return normalize(normal_in);
#endif
}

vec3 DecodeNormalMapNormal(vec3 normal_sample) {
#if COMPRESS_NORMAL_MAP
#if NORMAL_MAP_ENCODING_SPHEREMAP
    return DecodeNormalSpheremap(normal_sample.xy);
#elif NORMAL_MAP_ENCODING_OCT
    return DecodeNormalOct(normal_sample.xy);
#else
    return normalize(normal_sample * 2.0 - 1.0);
#endif
#else
    return normalize(normal_sample * 2.0 - 1.0);
#endif
}

vec2 EncodeGBufferNormal(vec3 n) {
#if COMPRESS_GBUFFER_NORMAL
#if GBUFFER_NORMAL_ENCODING_SPHEREMAP
    return EncodeNormalSpheremap(n);
#elif GBUFFER_NORMAL_ENCODING_OCT
    return EncodeNormalOct(n);
#else
    return normalize(n).xy;
#endif
#else
    return normalize(n).xy;
#endif
}

vec3 DecodeGBufferNormal(vec2 packed_n) {
#if COMPRESS_GBUFFER_NORMAL
#if GBUFFER_NORMAL_ENCODING_SPHEREMAP
    return DecodeNormalSpheremap(packed_n);
#elif GBUFFER_NORMAL_ENCODING_OCT
    return DecodeNormalOct(packed_n);
#else
    return normalize(vec3(packed_n, sqrt(max(0.0, 1.0 - dot(packed_n, packed_n)))));
#endif
#else
    return normalize(vec3(packed_n, sqrt(max(0.0, 1.0 - dot(packed_n, packed_n)))));
#endif
}

)";
}

static bool ContainsName(const std::vector<std::string> &names, const char *name)
{
    if (!name || !*name)
        return false;

    for (const auto &item : names)
    {
        if (item == name)
            return true;
    }

    return false;
}

static void AppendUniqueName(std::vector<std::string> &names, const char *name)
{
    if (!name || !*name)
        return;

    if (!ContainsName(names, name))
        names.emplace_back(name);
}

static void CollectRequiredNamesFromLogicBlock(
    const ShaderLogicBlock &block,
    std::vector<std::string> &required_resources,
    std::vector<std::string> &required_helpers)
{
    if (block.required_resources && block.required_resource_count > 0)
    {
        for (uint32_t i = 0; i < block.required_resource_count; ++i)
            AppendUniqueName(required_resources, block.required_resources[i]);
    }

    if (block.required_helpers && block.required_helper_count > 0)
    {
        for (uint32_t i = 0; i < block.required_helper_count; ++i)
            AppendUniqueName(required_helpers, block.required_helpers[i]);
    }
}

bool BuildComposedMaterialDefFromLogic(
    const ComposedMaterialDef &base_def,
    const MaterialLogicDef &logic,
    ComposedMaterialBuildFromLogicResult &out)
{
    out.filtered_descriptors.clear();
    out.diagnostics.missing_resources.clear();
    out.def.logic_required_helpers.clear();

    std::vector<std::string> required_resources;
    std::vector<std::string> required_helpers;
    CollectRequiredNamesFromLogicBlock(logic.vertex, required_resources, required_helpers);
    CollectRequiredNamesFromLogicBlock(logic.fragment, required_resources, required_helpers);

    out.def = base_def;

    out.vertex_business = base_def.vertex_business ? *base_def.vertex_business : VertexShaderBusiness{nullptr};
    out.fragment_business = base_def.fragment_business ? *base_def.fragment_business : FragmentShaderBusiness{nullptr};

    if (logic.vertex.main_logic)
        out.vertex_business.code = logic.vertex.main_logic;

    if (logic.fragment.main_logic)
        out.fragment_business.code = logic.fragment.main_logic;

    out.def.vertex_business = &out.vertex_business;
    out.def.fragment_business = &out.fragment_business;

    for (const auto &required_name : required_resources)
    {
        const FixedDescriptorEntry *entry = FindDescriptorByName(base_def, required_name.c_str());
        if (entry)
        {
            out.filtered_descriptors.push_back(*entry);
        }
        else
        {
            out.diagnostics.missing_resources.emplace_back(required_name);
        }
    }

    if (!out.filtered_descriptors.empty())
    {
        out.def.descriptor_entries = out.filtered_descriptors.data();
        out.def.descriptor_entry_count = uint32_t(out.filtered_descriptors.size());
    }
    else if (!required_resources.empty())
    {
        out.def.descriptor_entries = nullptr;
        out.def.descriptor_entry_count = 0;
    }

    out.def.logic_required_helpers = required_helpers;

    return out.diagnostics.missing_resources.empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 1: 生成前置部分（版本 + 宏定义）
// ─────────────────────────────────────────────────────────────────────────────

static std::string GenPreamble(const ShaderPermutationKey &key)
{
    std::string result;
    result += "#version 450 core\n";
    result += "#extension GL_ARB_gpu_shader_int64 : enable\n";
    result += "\n";
    
    // 从 key 中注入 permutation 宏
    std::string defines;
    key.AppendGLSLDefines(defines);
    result += defines;
    result += "\n";
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 2: 生成顶点输入结构体
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenVertexInputStruct(const ComposedMaterialDef &def)
{
    std::string result;
    result += "struct VertexInput {\n";
    
    for (uint32_t i = 0; i < def.vertex_entry_count; i++) {
        const auto &entry = def.vertex_entries[i];
        const char *glsl_type = nullptr;
        
        // VAType 是结构体，其中 basetype 和 vec_size 决定了具体类型
        // 这里使用 GetVertexAttribName 辅助函数
        glsl_type = GetVertexAttribName(&entry.type);
        
        if (!glsl_type) {
            glsl_type = "vec4";  // fallback
        }

        const char *vertex_name = entry.name ? entry.name : "_unnamed";
        
        char buf[256];
        snprintf(buf, sizeof(buf), "    %s %s;\n", glsl_type, vertex_name);
        result += buf;
    }
    
    result += "};\n\n";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 3: 生成 VS_Output 结构体（从 VS 到 FS 的插值数据）
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenVSOutputStruct(const ComposedMaterialDef &def)
{
    std::string result;
    result += "struct VS_Output {\n";
    result += "    vec4 ClipPos;           // 隐式，写入 gl_Position\n";
    
    // 如果有 Position，生成 WorldPosition
    if (HasVertexAttribute(def, "Position")) {
        result += "    vec3 WorldPosition;     // 世界坐标\n";
    }
    
    // 如果有 Normal，生成 WorldNormal
    if (HasVertexAttribute(def, "Normal")) {
        result += "    vec3 WorldNormal;       // 世界法线\n";
    }
    
    // 如果有 TexCoord，转发
    if (HasVertexAttribute(def, "TexCoord")) {
        result += "    vec2 TexCoord;          // UV 坐标\n";
    }
    
    // 材质实例 ID（用于从 SSBO 读取材质数据）
    result += "    uint MaterialInstanceID; // 材质实例索引\n";

    // 前向顶点光照插值通道（低配/远景）
    result += "#if FORWARD_LIGHTING_PER_VERTEX\n";
    result += "    vec3 VertexLighting;    // 顶点光照插值\n";
    result += "#endif\n";
    
    result += "};\n\n";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 4: 生成光照输出结构体
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenLightingOutputStruct()
{
    std::string result;
    result += "struct LightingOutput {\n";
    result += "    vec3 diffuse;           // 漫反射颜色\n";
    result += "    vec3 specular;          // 高光颜色\n";
    result += "    vec3 reflection;        // 反射色（IBL 用）\n";
    result += "};\n\n";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 5: 生成布局声明和 uniform 块
// ─────────────────────────────────────────────────────────────────────────────

static std::string GenLayoutDeclarations(const ComposedMaterialDef &def)
{
    ResourceLayoutGenerator layout_gen;
    layout_gen.Reset();

    return layout_gen.GenDescriptorLayout(def.descriptor_entries, def.descriptor_entry_count);
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 6: 生成坐标变换基础函数
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenCoordinateTransformFunctions()
{
    std::string result;
    result += R"(
// 坐标变换辅助函数
vec4 GetLocalToWorldPos(vec4 local_pos) {
    return GetLocalToWorld() * local_pos;
}

vec4 GetClipSpacePos(vec4 world_pos) {
    return ViewProj * world_pos;
}

vec4 GetScreenSpacePos(vec4 clip_pos) {
    return clip_pos;  // 隐式除以 w
}

)";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 关键部分：生成辅助函数库
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenHelperFunctionLibrary(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const char *shader_stage)
{
    std::string result;
    
    // 所有 stage 都需要
    result += GenGetLocalToWorld(def);
    result += GenGetNormalMatrix(def);
    
    // Stage-specific 部分
    if (CStrEq(shader_stage, "VS")) {
        result += GenGetNormalFunction(def, "VS");
        result += GenGetPositionFunctions(def, "VS");
        result += GenGetMaterialInstanceFunctions(def, "VS");
    }
    else if (CStrEq(shader_stage, "GS")) {
        result += GenGetNormalFunction(def, "GS");
        result += GenGetPositionFunctions(def, "GS");
        result += GenGetMaterialInstanceFunctions(def, "GS");
    }
    else if (CStrEq(shader_stage, "FS")) {
        result += GenGetNormalFunction(def, "FS");
        result += GenGetPositionFunctions(def, "FS");
        result += GenGetMaterialInstanceFunctions(def, "FS");
    }

    // Stage 3: 框架统一内置 helper 库（高级函数）
    result += builtin_helpers::GenStageHelpers(def, key, shader_stage);
    
    return result;
}

std::string ComposedShaderGenerator::GenGetLocalToWorld(const ComposedMaterialDef &def)
{
    std::string result;
    
    // 查找 LocalToWorld 描述符
    const auto *l2w_desc = FindDescriptorByName(def, "LocalToWorld");

    if (l2w_desc || FindDescriptorByName(def, "l2w")) {
        const bool has_transform_id = HasVertexAttribute(def, "TransformID");

        if (has_transform_id)
        {
            result += R"(
mat4 GetLocalToWorld() {
    return l2w.mats[TransformID];
}

)";
        }
        else
        {
            result += R"(
mat4 GetLocalToWorld() {
    return LocalToWorld;  // 从 LocalToWorld UBO 读取
}

)";
        }
    }
    
    return result;
}

std::string ComposedShaderGenerator::GenGetNormalMatrix(const ComposedMaterialDef &def)
{
    std::string result;
    result += R"(
mat3 GetNormalMatrix() {
    // = transpose(inverse(mat3(ViewMatrix * LocalToWorld)))
    // 框架简化为直接计算
    return mat3(GetLocalToWorld());
}

)";
    return result;
}

std::string ComposedShaderGenerator::GenGetNormalFunction(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    std::string result;
    
    bool has_normal = HasVertexAttribute(def, "Normal");
    
    if (CStrEq(shader_stage, "VS")) {
        // VS 中生成两个版本
        if (has_normal) {
            result += R"(
vec3 GetNormal() {
    return normalize(GetNormalMatrix() * DecodeVertexInputNormal(Normal));
}

)";
        }
        
        result += R"(
vec3 GetNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}

)";
    }
    else if (CStrEq(shader_stage, "FS")) {
        // FS 中返回插值的世界法线
        result += R"(
vec3 GetNormal() {
    return normalize(Input.WorldNormal);
}

vec3 GetWorldNormal() {
    return normalize(Input.WorldNormal);
}

vec3 DecodeMaterialNormal(vec3 normal_sample) {
    return DecodeNormalMapNormal(normal_sample);
}

)";
    }
    
    return result;
}

std::string ComposedShaderGenerator::GenGetPositionFunctions(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    std::string result;
    
    if (CStrEq(shader_stage, "VS")) {
        result += R"(
vec4 GetPosition3D() {
    return GetLocalToWorld() * vec4(Position, 1.0);
}

vec4 GetClipPosition() {
    return ViewProj * GetPosition3D();
}

)";
    }
    else if (CStrEq(shader_stage, "FS")) {
        result += R"(
vec4 GetPosition3D() {
    return vec4(Input.WorldPosition, 1.0);
}

vec3 GetWorldPosition() {
    return Input.WorldPosition;
}

)";
    }
    
    return result;
}

std::string ComposedShaderGenerator::GenGetMaterialInstanceFunctions(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    std::string result;
    
    // 查找 MaterialInstanceData 描述符
    const FixedDescriptorEntry *mi_desc = nullptr;
    for (uint32_t i = 0; i < def.descriptor_entry_count; i++) {
        const char *desc_name = def.descriptor_entries[i].name;
        if (desc_name &&
            (CStrEq(desc_name, "MaterialInstanceData") ||
             CStrEq(desc_name, "mtl"))) {
            mi_desc = &def.descriptor_entries[i];
            break;
        }
    }
    
    if (mi_desc) {
        if (CStrEq(shader_stage, "VS")) {
            result += R"(
MaterialInstance GetMaterialInstance() {
    return mi.mi[MaterialInstanceID];
}

MaterialInstance GetMI() {
    return GetMaterialInstance();
}

)";
        }
        else if (CStrEq(shader_stage, "FS")) {
            result += R"(
MaterialInstance GetMaterialInstance() {
    return mtl.mi[Input.MaterialInstanceID];
}

MaterialInstance GetMI() {
    return GetMaterialInstance();
}

)";
        }
    }
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 7: 生成输出合成代码
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenOutputCompositionCode(ShaderOutputMode mode)
{
    std::string result;
    
    switch (mode) {
        case ShaderOutputMode::SingleRTAlphaBlend:
            result += R"(
void ComposeFinalOutput(vec4 color_with_alpha, out vec4 out_rt0) {
    out_rt0 = color_with_alpha;
    // Blend 由 pipeline 设置为 ONE_MINUS_SRC_ALPHA
}

)";
            break;
            
        case ShaderOutputMode::SingleRTAdditive:
            result += R"(
void ComposeFinalOutput(vec4 color_with_alpha, out vec4 out_rt0) {
    out_rt0 = vec4(color_with_alpha.rgb, 0.0);  // 忽略 alpha
    // Blend 由 pipeline 设置为 ONE_ONE
}

)";
            break;
            
        case ShaderOutputMode::DualRTDeferred:
            result += R"(
void ComposeFinalOutput(vec4 color_with_alpha, out vec4 out_rt0, out vec4 out_rt1) {
    out_rt0 = vec4(color_with_alpha.rgb, 1.0);   // Diffuse color
    vec2 packed_n = EncodeGBufferNormal(GetWorldNormal());
    out_rt1 = vec4(packed_n, 0.0, 1.0);          // packed normal + reserved
}

)";
            break;
            
        default:
            result += "// 输出合成代码（自定义）\n";
    }
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 8: 生成光照计算代码（占位符，实现延迟至 M2-M3）
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::GenLightingCode(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key)
{
    std::string result;
    
    if (!def.enable_lighting) {
        return "// 光照禁用，无计算代码\n";
    }
    
    // TODO: M2-M3 阶段实现完整的光照计算代码生成
    result += R"(
// 占位符：光照计算将在 M2-M3 阶段实现
LightingOutput ComputeLighting(vec3 normal, vec3 albedo, vec3 view_dir) {
    LightingOutput out;
    out.diffuse = albedo * 0.5;      // 临时 Lambert
    out.specular = vec3(0.0);
    out.reflection = vec3(0.0);
    return out;
}

)";
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主入口：生成完整顶点着色器
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::ComposeVertexShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const bool include_preamble)
{
    std::string result;
    
    // Step 1: 前置部分
    if (include_preamble)
    {
        result += GenPreamble(key);
        PipelineMode default_pipeline_mode;
        result += GenNormalCompressionDefines(default_pipeline_mode);
        result += GenNormalCompressionHelpers();
    }
    
    // Step 2: 布局声明
    result += GenLayoutDeclarations(def);
    
    // Step 3: 结构体定义
    result += GenVertexInputStruct(def);
    result += GenVSOutputStruct(def);
    result += GenLightingOutputStruct();
    if (def.mi_glsl_codes)
    {
        result += def.mi_glsl_codes;  // 材质实例结构体
        result += "\n";
    }
    
    // Step 4: 坐标变换基础函数
    result += GenCoordinateTransformFunctions();
    
    // Step 5: 辅助函数库（关键部分）
    result += GenHelperFunctionLibrary(def, key, "VS");
    
    // Step 6: 开发者业务代码
    if (def.vertex_business && def.vertex_business->code) {
        result += def.vertex_business->code;
        result += "\n\n";
    }
    
    // Step 7: Main 函数
    result += R"(
void main() {
    VertexInput vi;
    vi.Position = Position;
)";
    
    if (HasVertexAttribute(def, "Normal")) {
        result += "    vi.Normal = Normal;\n";
    }
    if (HasVertexAttribute(def, "TexCoord")) {
        result += "    vi.TexCoord = TexCoord;\n";
    }
    
    result += R"(
    vec4 local_pos = VertexShaderBusiness(vi);
    gl_Position = GetClipPosition();
    
    VS_Output vso;
    vso.ClipPos = gl_Position;
)";
    
    if (HasVertexAttribute(def, "Position")) {
        result += "    vso.WorldPosition = GetPosition3D().xyz;\n";
    }
    if (HasVertexAttribute(def, "Normal")) {
        result += "    vso.WorldNormal = GetNormal();\n";
    }
    if (HasVertexAttribute(def, "TexCoord")) {
        result += "    vso.TexCoord = TexCoord;\n";
    }
    
    result += "    vso.MaterialInstanceID = MaterialInstanceID;\n";
    result += R"(
#if FORWARD_LIGHTING_PER_VERTEX
    vec3 _vertex_light = vec3(1.0, 1.0, 1.0);
)";
    if (HasVertexAttribute(def, "Normal")) {
        result += R"(
    vec3 _n = normalize(vso.WorldNormal);
    vec3 _l = normalize(vec3(0.2, 0.8, 0.4));
    float _half_lambert = dot(_n, _l) * 0.5 + 0.5;
    _vertex_light = vec3(_half_lambert);
)";
    }
    result += R"(
    vso.VertexLighting = _vertex_light;
#endif
)";
    result += "}\n";
    
    return result;
}

std::string ComposedShaderGenerator::ComposeVertexShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const PipelineMode &pipeline_mode,
    const bool include_preamble)
{
    return ComposeVertexShaderWithDiagnostics(def, key, pipeline_mode, include_preamble).code;
}

ShaderComposeResult ComposedShaderGenerator::ComposeVertexShaderWithDiagnostics(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const PipelineMode &pipeline_mode,
    const bool include_preamble)
{
    const PipelineMode resolved_mode = ResolvePipelineModeForCurrentBackend(pipeline_mode);
    const NormalCompressionNormalizationDiagnostics normalization_diagnostics =
        BuildNormalCompressionNormalizationDiagnostics(pipeline_mode, resolved_mode);

    ShaderComposeResult output;
    output.diagnostics = ToPublicDiagnostics(normalization_diagnostics);

    if (resolved_mode.render_path == PipelineRenderPath::MobileSubpassGBufferDeferred)
    {
        std::string result;
        if (include_preamble)
        {
            result += GenPreamble(key);
            result += GenNormalCompressionNormalizationComments(normalization_diagnostics);
            result += GenNormalCompressionDefines(resolved_mode);
            result += GenNormalCompressionHelpers();
            result += "#define MOBILE_SUBPASS_GBUFFER 1\n";
            result += "#define MOBILE_SUBPASS_USE_SUBPASSLOAD 1\n\n";
        }

        result += ComposeVertexShader(def, key, false);
        result += "\n// MobileSubpassGBufferDeferred route (VS): geometry path unchanged, FS consumes subpass inputs.\n";
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    if (resolved_mode.render_path == PipelineRenderPath::Forward
     && resolved_mode.forward_lighting == PipelineForwardLightingMode::PerVertex)
    {
        std::string result;
        if (include_preamble)
        {
            result += GenPreamble(key);
            result += GenNormalCompressionNormalizationComments(normalization_diagnostics);
            result += GenNormalCompressionDefines(resolved_mode);
            result += GenNormalCompressionHelpers();
            result += "#define FORWARD_LIGHTING_PER_VERTEX 1\n";
            result += "#define FORWARD_LIGHTING_PER_PIXEL 0\n\n";
        }

        result += ComposeVertexShader(def, key, false);
        result += "\n// Forward lighting mode: PerVertex (SG-2 placeholder route).\n";
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    if (resolved_mode.topology == PipelineTopology::MeshFS)
    {
        std::string result;
        if (include_preamble)
            result += GenPreamble(key);

        result += "// Mesh/FS topology selected: vertex shader stage is not used.\n";
        result += "// SG-2: mesh shader generation will be emitted by ComposeMeshShader().\n";
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    // 当前实现：VS/FS 路径复用 legacy 生成逻辑
    if (include_preamble)
    {
        std::string result;
        result += GenPreamble(key);
        result += GenNormalCompressionNormalizationComments(normalization_diagnostics);
        result += GenNormalCompressionDefines(resolved_mode);
        result += GenNormalCompressionHelpers();
        result += ComposeVertexShader(def, key, false);
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    output.code = ComposeVertexShader(def, key, include_preamble);
    CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
    return output;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主入口：生成完整片元着色器
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::ComposeFragmentShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const bool include_preamble)
{
    std::string result;
    
    // 前置部分
    if (include_preamble)
    {
        result += GenPreamble(key);
        PipelineMode default_pipeline_mode;
        result += GenNormalCompressionDefines(default_pipeline_mode);
        result += GenNormalCompressionHelpers();
    }
    
    // 布局声明
    result += GenLayoutDeclarations(def);
    
    // 结构体定义
    result += GenVertexInputStruct(def);
    result += GenVSOutputStruct(def);
    result += GenLightingOutputStruct();
    if (def.mi_glsl_codes)
    {
        result += def.mi_glsl_codes;
        result += "\n";
    }
    
    // 坐标变换基础函数
    result += GenCoordinateTransformFunctions();
    
    // 辅助函数库
    result += GenHelperFunctionLibrary(def, key, "FS");
    
    // 光照计算（如果启用）
    if (def.enable_lighting) {
        result += GenLightingCode(def, key);
    }
    
    // 输出合成代码
    result += GenOutputCompositionCode(def.output_mode);
    
    // 天光辅助函数（header + 模型选择实现）— 在业务代码调用它们之前注入
    result += SKYLIGHT_GLSL_HEADER;
    if (const char *sky_model_impl = GetSkyLightModelImplGLSL(key.ambient))
        result += sky_model_impl;
    result += "\n";

    // 开发者业务代码
    if (def.fragment_business && def.fragment_business->code) {
        result += def.fragment_business->code;
        result += "\n\n";
    }
    
    // Main 函数
    result += R"(
void main() {
    VS_Output vso = Input;
    
    vec4 business_output = FragmentShaderBusiness(vso);

#if FORWARD_LIGHTING_PER_VERTEX
    business_output.rgb *= clamp(vso.VertexLighting, vec3(0.0), vec3(1.0));
#endif
    
    vec4 out_rt0;
    ComposeFinalOutput(business_output, out_rt0);
    
    OutColor = out_rt0;
}
)";
    
    return result;
}

std::string ComposedShaderGenerator::ComposeFragmentShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const PipelineMode &pipeline_mode,
    const bool include_preamble)
{
    return ComposeFragmentShaderWithDiagnostics(def, key, pipeline_mode, include_preamble).code;
}

ShaderComposeResult ComposedShaderGenerator::ComposeFragmentShaderWithDiagnostics(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const PipelineMode &pipeline_mode,
    const bool include_preamble)
{
    const PipelineMode resolved_mode = ResolvePipelineModeForCurrentBackend(pipeline_mode);
    const NormalCompressionNormalizationDiagnostics normalization_diagnostics =
        BuildNormalCompressionNormalizationDiagnostics(pipeline_mode, resolved_mode);

    ShaderComposeResult output;
    output.diagnostics = ToPublicDiagnostics(normalization_diagnostics);

    if (resolved_mode.render_path == PipelineRenderPath::MobileSubpassGBufferDeferred)
    {
        std::string result;
        if (include_preamble)
        {
            result += GenPreamble(key);
            result += GenNormalCompressionNormalizationComments(normalization_diagnostics);
            result += GenNormalCompressionDefines(resolved_mode);
            result += GenNormalCompressionHelpers();
            result += "#define MOBILE_SUBPASS_GBUFFER 1\n";
            result += "#define MOBILE_SUBPASS_USE_SUBPASSLOAD 1\n\n";
        }

        result += R"(
// Mobile subpass GBuffer input template (SG-2 placeholder)
// layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput GBufferInput0;
// layout(input_attachment_index=1, set=0, binding=1) uniform subpassInput GBufferInput1;
// vec4 ReadGBuffer0() { return subpassLoad(GBufferInput0); }
// vec4 ReadGBuffer1() { return subpassLoad(GBufferInput1); }

)";

        result += ComposeFragmentShader(def, key, false);
        result += "\n// MobileSubpassGBufferDeferred route (FS): subpassLoad input path enabled.\n";
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    if (resolved_mode.render_path == PipelineRenderPath::Forward
     && resolved_mode.forward_lighting == PipelineForwardLightingMode::PerVertex)
    {
        std::string result;
        if (include_preamble)
        {
            result += GenPreamble(key);
            result += GenNormalCompressionNormalizationComments(normalization_diagnostics);
            result += GenNormalCompressionDefines(resolved_mode);
            result += GenNormalCompressionHelpers();
            result += "#define FORWARD_LIGHTING_PER_VERTEX 1\n";
            result += "#define FORWARD_LIGHTING_PER_PIXEL 0\n\n";
        }

        result += ComposeFragmentShader(def, key, false);
        result += "\n// Forward lighting mode: PerVertex (expect interpolated vertex-lighting input in SG-2).\n";
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    if (resolved_mode.topology == PipelineTopology::MeshFS)
    {
        std::string result;
        if (include_preamble)
            result += GenPreamble(key);

        result += "// Mesh/FS topology selected: fragment stage shares FS composer path.\n";
        result += ComposeFragmentShader(def, key, false);
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    if (include_preamble)
    {
        std::string result;
        result += GenPreamble(key);
        result += GenNormalCompressionNormalizationComments(normalization_diagnostics);
        result += GenNormalCompressionDefines(resolved_mode);
        result += GenNormalCompressionHelpers();
        result += ComposeFragmentShader(def, key, false);
        output.code = result;
        CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
        return output;
    }

    output.code = ComposeFragmentShader(def, key, include_preamble);
    CollectHelperConflictDiagnosticsFromCode(output.code, output.diagnostics);
    return output;
}

// ─────────────────────────────────────────────────────────────────────────────
// 几何着色器（占位符）
// ─────────────────────────────────────────────────────────────────────────────

std::string ComposedShaderGenerator::ComposeGeometryShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const bool include_preamble)
{
    // TODO: M2-M3 实现几何着色器生成
    if (include_preamble)
        return "#version 450 core\n\n// 几何着色器生成延迟至 M2-M3\n";
    return "// 几何着色器生成延迟至 M2-M3\n";
}

std::string ComposedShaderGenerator::ComposeMeshShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key,
    const PipelineMode &pipeline_mode,
    const bool include_preamble)
{
    const PipelineMode resolved_mode = ResolvePipelineModeForCurrentBackend(pipeline_mode);

    std::string result;
    if (include_preamble)
        result += GenPreamble(key);

    (void)def;
    (void)resolved_mode;

    result += "// Mesh shader generation placeholder (SG-2).\n";
    result += "// This entry exists for topology-adaptive routing (VS/FS <-> Mesh/FS).\n";
    return result;
}

}  // namespace hgl::graph::mtl
