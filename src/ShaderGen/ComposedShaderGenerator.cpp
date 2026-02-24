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

#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/type/String.h>
#include <hgl/graph/mtl/StdMaterial.h>

namespace hgl::graph::mtl {

// ─────────────────────────────────────────────────────────────────────────────
// 工具函数：顶点属性检查
// ─────────────────────────────────────────────────────────────────────────────

static bool HasVertexAttribute(
    const ComposedMaterialDef &def,
    const char *name)
{
    for (uint32_t i = 0; i < def.vertex_entry_count; i++) {
        const auto &entry = def.vertex_entries[i];
        if (strcmp(entry.name, name) == 0) {
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
        if (strcmp(entry.name, name) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 1: 生成前置部分（版本 + 宏定义）
// ─────────────────────────────────────────────────────────────────────────────

static AnsiString GenPreamble(const ShaderPermutationKey &key)
{
    AnsiString result;
    result += "#version 450 core\n";
    result += "#extension GL_ARB_gpu_shader_int64 : enable\n";
    result += "\n";
    
    // 从 key 中注入 permutation 宏
    AnsiString defines;
    key.AppendGLSLDefines(defines);
    result += defines;
    result += "\n";
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 2: 生成顶点输入结构体
// ─────────────────────────────────────────────────────────────────────────────

AnsiString ComposedShaderGenerator::GenVertexInputStruct(const ComposedMaterialDef &def)
{
    AnsiString result;
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
        
        char buf[256];
        snprintf(buf, sizeof(buf), "    %s %s;\n", glsl_type, entry.name);
        result += buf;
    }
    
    result += "};\n\n";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 3: 生成 VS_Output 结构体（从 VS 到 FS 的插值数据）
// ─────────────────────────────────────────────────────────────────────────────

AnsiString ComposedShaderGenerator::GenVSOutputStruct(const ComposedMaterialDef &def)
{
    AnsiString result;
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
    
    result += "};\n\n";
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 4: 生成光照输出结构体
// ─────────────────────────────────────────────────────────────────────────────

AnsiString ComposedShaderGenerator::GenLightingOutputStruct()
{
    AnsiString result;
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

static AnsiString GenLayoutDeclarations(const ComposedMaterialDef &def)
{
    AnsiString result;
    
    // 遍历所有描述符，生成对应的 layout 声明
    for (uint32_t i = 0; i < def.descriptor_entry_count; i++) {
        const auto &desc = def.descriptor_entries[i];
        
        char buf[512];
        
        switch (desc.kind) {
            case DescriptorKind::UBO: {
                snprintf(buf, sizeof(buf),
                    "layout(set=0, binding=%u) uniform %s {\n"
                    "    // 数据将在运行时填充\n"
                    "    vec4 _placeholder;\n"
                    "} %s;\n\n",
                    i, desc.struct_name ? desc.struct_name : "UniformBlock", desc.name);
                result += buf;
                break;
            }
            
            case DescriptorKind::SSBO: {
                snprintf(buf, sizeof(buf),
                    "layout(set=0, binding=%u, std430) buffer %s {\n"
                    "    // 数据将在运行时填充\n"
                    "    vec4 _data[];\n"
                    "} %s;\n\n",
                    i, desc.struct_name ? desc.struct_name : "StorageBlock", desc.name);
                result += buf;
                break;
            }
            
            case DescriptorKind::Texture: {
                snprintf(buf, sizeof(buf),
                    "layout(set=0, binding=%u) uniform sampler2D %s;\n\n",
                    i, desc.name);
                result += buf;
                break;
            }
                
            case DescriptorKind::TextureSampler:
                // 通常与 Texture 一起，这里跳过或生成采样器
                break;
        }
    }
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Step 6: 生成坐标变换基础函数
// ─────────────────────────────────────────────────────────────────────────────

AnsiString ComposedShaderGenerator::GenCoordinateTransformFunctions()
{
    AnsiString result;
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

AnsiString ComposedShaderGenerator::GenHelperFunctionLibrary(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    AnsiString result;
    
    // 所有 stage 都需要
    result += GenGetLocalToWorld(def);
    result += GenGetNormalMatrix(def);
    
    // Stage-specific 部分
    if (strcmp(shader_stage, "VS") == 0) {
        result += GenGetNormalFunction(def, "VS");
        result += GenGetPositionFunctions(def, "VS");
        result += GenGetMaterialInstanceFunctions(def, "VS");
    }
    else if (strcmp(shader_stage, "GS") == 0) {
        result += GenGetNormalFunction(def, "GS");
        result += GenGetPositionFunctions(def, "GS");
        result += GenGetMaterialInstanceFunctions(def, "GS");
    }
    else if (strcmp(shader_stage, "FS") == 0) {
        result += GenGetNormalFunction(def, "FS");
        result += GenGetPositionFunctions(def, "FS");
        result += GenGetMaterialInstanceFunctions(def, "FS");
    }
    
    return result;
}

AnsiString ComposedShaderGenerator::GenGetLocalToWorld(const ComposedMaterialDef &def)
{
    AnsiString result;
    
    // 查找 LocalToWorld 描述符
    const auto *l2w_desc = FindDescriptorByName(def, "LocalToWorld");
    
    if (l2w_desc || FindDescriptorByName(def, "l2w")) {
        result += R"(
mat4 GetLocalToWorld() {
    return LocalToWorld;  // 从 LocalToWorld UBO 读取
}

)";
    }
    
    return result;
}

AnsiString ComposedShaderGenerator::GenGetNormalMatrix(const ComposedMaterialDef &def)
{
    AnsiString result;
    result += R"(
mat3 GetNormalMatrix() {
    // = transpose(inverse(mat3(ViewMatrix * LocalToWorld)))
    // 框架简化为直接计算
    return mat3(GetLocalToWorld());
}

)";
    return result;
}

AnsiString ComposedShaderGenerator::GenGetNormalFunction(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    AnsiString result;
    
    bool has_normal = HasVertexAttribute(def, "Normal");
    
    if (strcmp(shader_stage, "VS") == 0) {
        // VS 中生成两个版本
        if (has_normal) {
            result += R"(
vec3 GetNormal() {
    return normalize(GetNormalMatrix() * Normal);
}

)";
        }
        
        result += R"(
vec3 GetNormal(vec3 local_normal) {
    return normalize(GetNormalMatrix() * local_normal);
}

)";
    }
    else if (strcmp(shader_stage, "FS") == 0) {
        // FS 中返回插值的世界法线
        result += R"(
vec3 GetNormal() {
    return Input.WorldNormal;
}

vec3 GetWorldNormal() {
    return Input.WorldNormal;
}

)";
    }
    
    return result;
}

AnsiString ComposedShaderGenerator::GenGetPositionFunctions(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    AnsiString result;
    
    if (strcmp(shader_stage, "VS") == 0) {
        result += R"(
vec4 GetPosition3D() {
    return GetLocalToWorld() * vec4(Position, 1.0);
}

vec4 GetClipPosition() {
    return ViewProj * GetPosition3D();
}

)";
    }
    else if (strcmp(shader_stage, "FS") == 0) {
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

AnsiString ComposedShaderGenerator::GenGetMaterialInstanceFunctions(
    const ComposedMaterialDef &def,
    const char *shader_stage)
{
    AnsiString result;
    
    // 查找 MaterialInstanceData 描述符
    const FixedDescriptorEntry *mi_desc = nullptr;
    for (uint32_t i = 0; i < def.descriptor_entry_count; i++) {
        if (strcmp(def.descriptor_entries[i].name, "MaterialInstanceData") == 0 ||
            strcmp(def.descriptor_entries[i].name, "mtl") == 0) {
            mi_desc = &def.descriptor_entries[i];
            break;
        }
    }
    
    if (mi_desc) {
        if (strcmp(shader_stage, "VS") == 0) {
            result += R"(
MaterialInstance GetMaterialInstance() {
    return mi.mi[MaterialInstanceID];
}

MaterialInstance GetMI() {
    return GetMaterialInstance();
}

)";
        }
        else if (strcmp(shader_stage, "FS") == 0) {
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

AnsiString ComposedShaderGenerator::GenOutputCompositionCode(ShaderOutputMode mode)
{
    AnsiString result;
    
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
    out_rt1 = vec4(GetWorldNormal(), 1.0);       // Normal + material id
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

AnsiString ComposedShaderGenerator::GenLightingCode(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key)
{
    AnsiString result;
    
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

AnsiString ComposedShaderGenerator::ComposeVertexShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key)
{
    AnsiString result;
    
    // Step 1: 前置部分
    result += GenPreamble(key);
    
    // Step 2: 布局声明
    result += GenLayoutDeclarations(def);
    
    // Step 3: 结构体定义
    result += GenVertexInputStruct(def);
    result += GenVSOutputStruct(def);
    result += GenLightingOutputStruct();
    result += def.mi_glsl_codes;  // 材质实例结构体
    result += "\n";
    
    // Step 4: 坐标变换基础函数
    result += GenCoordinateTransformFunctions();
    
    // Step 5: 辅助函数库（关键部分）
    result += GenHelperFunctionLibrary(def, "VS");
    
    // Step 6: 开发者业务代码
    if (def.vertex_business) {
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
    result += "}\n";
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 主入口：生成完整片元着色器
// ─────────────────────────────────────────────────────────────────────────────

AnsiString ComposedShaderGenerator::ComposeFragmentShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key)
{
    AnsiString result;
    
    // 前置部分
    result += GenPreamble(key);
    
    // 布局声明
    result += GenLayoutDeclarations(def);
    
    // 结构体定义
    result += GenVertexInputStruct(def);
    result += GenVSOutputStruct(def);
    result += GenLightingOutputStruct();
    result += def.mi_glsl_codes;
    result += "\n";
    
    // 坐标变换基础函数
    result += GenCoordinateTransformFunctions();
    
    // 辅助函数库
    result += GenHelperFunctionLibrary(def, "FS");
    
    // 光照计算（如果启用）
    if (def.enable_lighting) {
        result += GenLightingCode(def, key);
    }
    
    // 输出合成代码
    result += GenOutputCompositionCode(def.output_mode);
    
    // 开发者业务代码
    if (def.fragment_business) {
        result += def.fragment_business->code;
        result += "\n\n";
    }
    
    // Main 函数
    result += R"(
void main() {
    VS_Output vso = Input;
    
    vec4 business_output = FragmentShaderBusiness(vso);
    
    vec4 out_rt0;
    ComposeFinalOutput(business_output, out_rt0);
    
    OutColor = out_rt0;
}
)";
    
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// 几何着色器（占位符）
// ─────────────────────────────────────────────────────────────────────────────

AnsiString ComposedShaderGenerator::ComposeGeometryShader(
    const ComposedMaterialDef &def,
    const ShaderPermutationKey &key)
{
    // TODO: M2-M3 实现几何着色器生成
    return "// 几何着色器生成延迟至 M2-M3\n";
}

}  // namespace hgl::graph::mtl
