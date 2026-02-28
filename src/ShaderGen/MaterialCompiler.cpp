/// MaterialCompiler.cpp — FixedMaterialDef → MaterialCreateInfo 编译器实现
///
/// 三步流程：
///   1. 从 FixedDescriptorEntry[] 构建 MaterialDescriptorInfo（描述符布局）
///   2. 从 FixedVertexEntry[] 设置顶点输入
///   3. 使用 ShaderPermutationKey 的宏前缀编译 GLSL + 设置 MI 代码

#include <hgl/graph/mtl/MaterialCompiler.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/ShaderDescriptorInfo.h>
#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/ShaderCreateInfoFragment.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/vk/VKDeviceAttribute.h>
#include <hgl/type/String.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "common/MFCommon.h"
#include "common/MFGetPosition.h"
#include "common/MFGetNormal.h"

namespace hgl::graph::mtl {

static bool HasVertexEntry(const FixedMaterialDef &def, const char *name)
{
    if (!name || !*name)
        return false;

    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        if (def.vertex_entries[i].name && std::strcmp(def.vertex_entries[i].name, name) == 0)
            return true;
    }

    return false;
}

static bool IsPositionVec2(const FixedMaterialDef &def)
{
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        const auto &entry = def.vertex_entries[i];
        if (!entry.name)
            continue;

        if (std::strcmp(entry.name, VAN::Position) == 0)
            return entry.type.ToCode() == VAT_VEC2.ToCode();
    }

    return false;
}

static bool HasDescriptorNamed(const FixedMaterialDef &def, const char *name)
{
    if (!name || !*name)
        return false;

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const auto &entry = def.descriptor_entries[i];
        if ((entry.name && std::strcmp(entry.name, name) == 0)
         || (entry.struct_name && std::strcmp(entry.struct_name, name) == 0))
            return true;
    }

    return false;
}

static bool HasVertexEntry(const ComposedMaterialDef &def, const char *name)
{
    if (!name || !*name)
        return false;

    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        if (def.vertex_entries[i].name && std::strcmp(def.vertex_entries[i].name, name) == 0)
            return true;
    }

    return false;
}

static bool HasDescriptorNamed(const ComposedMaterialDef &def, const char *name)
{
    if (!name || !*name)
        return false;

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const auto &entry = def.descriptor_entries[i];
        if ((entry.name && std::strcmp(entry.name, name) == 0)
         || (entry.struct_name && std::strcmp(entry.struct_name, name) == 0))
            return true;
    }

    return false;
}

static bool HasPerMaterialDescriptor(const FixedMaterialDef &def)
{
    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        if (def.descriptor_entries[i].set_type == DescriptorSetType::PerMaterial)
            return true;
    }

    return false;
}

static const ShaderBufferSource *ResolveShaderBufferSourceByStructName(const Material3DCreateConfig *cfg,const char *struct_name)
{
    if(cfg)
    {
        if(const ShaderBufferSource *sbs=cfg->FindPrivateShaderBufferSourceByStructName(struct_name))
            return sbs;
    }

    return FindShaderBufferSourceByStructName(struct_name);
}

static const char *VATypeToGLSL(const VAType &type)
{
    switch (type.basetype)
    {
        case VertexAttribBaseType::Float:
            switch (type.vec_size)
            {
                case 1: return "float";
                case 2: return "vec2";
                case 3: return "vec3";
                case 4: return "vec4";
                default: return "vec4";
            }
        case VertexAttribBaseType::UInt:
            switch (type.vec_size)
            {
                case 1: return "uint";
                case 2: return "uvec2";
                case 3: return "uvec3";
                case 4: return "uvec4";
                default: return "uint";
            }
        case VertexAttribBaseType::Int:
            switch (type.vec_size)
            {
                case 1: return "int";
                case 2: return "ivec2";
                case 3: return "ivec3";
                case 4: return "ivec4";
                default: return "int";
            }
        case VertexAttribBaseType::Bool:
            switch (type.vec_size)
            {
                case 1: return "bool";
                case 2: return "bvec2";
                case 3: return "bvec3";
                case 4: return "bvec4";
                default: return "bool";
            }
        default:
            return "vec4";
    }
}

/// 提取业务代码中的插值变量（简化版：基于字符串匹配）
static std::vector<std::pair<std::string, std::string>> ExtractInterpolatedVariables(const char *code)
{
    std::vector<std::pair<std::string, std::string>> variables; // {name, type}
    
    if (!code || !*code)
        return variables;
    
    std::string code_str(code);
    
    // 检测常见插值变量模式
    struct InterpolationPattern {
        const char *pattern;
        const char *name;
        const char *type;
    };
    
    InterpolationPattern patterns[] = {
        {"Output.Color", "Color", "vec4"},
        {"Output.Normal", "Normal", "vec3"},
        {"Output.Position", "Position", "vec4"},
        {"Output.WorldPosition", "WorldPosition", "vec4"},
        {"Output.TexCoord", "TexCoord", "vec2"},
        {"Output.Tangent", "Tangent", "vec3"},
        {"Output.Bitangent", "Bitangent", "vec3"},
    };
    
    for (const auto &p : patterns)
    {
        if (code_str.find(p.pattern) != std::string::npos)
        {
            // 避免重复添加
            bool exists = false;
            for (const auto &v : variables)
            {
                if (v.first == p.name)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                variables.push_back({p.name, p.type});
        }
    }
    
    return variables;
}

static AnsiString BuildVertexGLSLFromBusiness(const FixedMaterialDef &fixed_def, const ComposedMaterialDef &def)
{
    AnsiString glsl;

    auto IsPositionVec2Entry = [](const FixedVertexEntry &entry) -> bool
    {
        if (!entry.name)
            return false;

        if (std::strcmp(entry.name, VAN::Position) != 0)
            return false;

        return entry.type.ToCode() == VAT_VEC2.ToCode();
    };

    // Step 1: 生成 VertexInput 结构体
    glsl += "struct VertexInput\n{\n";
    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        const auto &entry = def.vertex_entries[i];
        glsl += "    ";
        glsl += IsPositionVec2Entry(entry) ? "vec3" : VATypeToGLSL(entry.type);
        glsl += " ";
        glsl += entry.name;
        glsl += ";\n";
    }
    glsl += "};\n\n";

    // Step 2: 提取插值变量（从 VS business 代码中）
    std::vector<std::pair<std::string, std::string>> interp_vars;
    if (def.vertex_business && def.vertex_business->code)
    {
        interp_vars = ExtractInterpolatedVariables(def.vertex_business->code);
    }

    // Step 3: 生成业务用输出接口块（避免与 MI 输出的 Vertex_Output 冲突）
    if (!interp_vars.empty())
    {
        glsl += "layout(location=1) out Vertex_Business_Output\n{\n";
        for (const auto &v : interp_vars)
        {
            glsl += "    ";
            glsl += v.second.c_str(); // type
            glsl += " ";
            glsl += v.first.c_str(); // name
            glsl += ";\n";
        }
        glsl += "} BusinessOutput;\n\n";
        glsl += "#define Output BusinessOutput\n";
    }

    // Step 4: 插入 business 代码
    if (def.vertex_business && def.vertex_business->code)
    {
        glsl += def.vertex_business->code;
        glsl += "\n\n";
    }

    if (!interp_vars.empty())
        glsl += "#undef Output\n\n";

    // Step 5: 生成 main 函数
    const bool has_transform = HasVertexEntry(fixed_def, Assign::TransformID::VIS_NAME);
    const bool has_camera = HasDescriptorNamed(fixed_def, "camera") || HasDescriptorNamed(fixed_def, "CameraInfo");
    const bool has_material_instance = HasVertexEntry(fixed_def, Assign::MaterialInstanceID::VIS_NAME);

    glsl += R"(
void main()
{
    VertexInput vi;
)";

    for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
    {
        const auto &entry = def.vertex_entries[i];
        glsl += "    vi.";
        glsl += entry.name;
        glsl += "=";
        if (IsPositionVec2Entry(entry))
        {
            glsl += "vec3(";
            glsl += entry.name;
            glsl += ",0.0)";
        }
        else
            glsl += entry.name;
        glsl += ";\n";
    }

    if (has_material_instance)
        glsl += "    HandoverMI();\n";

    glsl += "    vec4 local_pos = VertexShaderBusiness(vi);\n";

    if (has_transform && has_camera)
        glsl += "    gl_Position = camera.vp * GetLocalToWorld() * local_pos;\n";
    else if (has_transform)
        glsl += "    gl_Position = GetLocalToWorld() * local_pos;\n";
    else if (has_camera)
        glsl += "    gl_Position = camera.vp * local_pos;\n";
    else
        glsl += "    gl_Position = local_pos;\n";

    glsl += "}\n";
    return glsl;
}

static AnsiString BuildFragmentGLSLFromBusiness(const FixedMaterialDef &fixed_def, const ComposedMaterialDef &def)
{
    AnsiString glsl;

    // Step 1: 提取插值变量（从 VS business 代码中，FS 需要匹配）
    std::vector<std::pair<std::string, std::string>> interp_vars;
    if (def.vertex_business && def.vertex_business->code)
    {
        interp_vars = ExtractInterpolatedVariables(def.vertex_business->code);
    }

    // Step 2: 生成业务用输入接口块（避免与 MI 输入的 Vertex_Output 冲突）
    if (!interp_vars.empty())
    {
        glsl += "layout(location=1) in Vertex_Business_Output\n{\n";
        for (const auto &v : interp_vars)
        {
            glsl += "    ";
            glsl += v.second.c_str(); // type
            glsl += " ";
            glsl += v.first.c_str(); // name
            glsl += ";\n";
        }
        glsl += "} BusinessInput;\n\n";
        glsl += "#define Input BusinessInput\n";
    }

    // Step 3: 插入 business 代码
    if (def.fragment_business && def.fragment_business->code)
    {
        glsl += def.fragment_business->code;
        glsl += "\n\n";
    }

    if (!interp_vars.empty())
        glsl += "#undef Input\n\n";

    // Step 4: 生成 main 函数
    glsl += "void main()\n{\n";
    glsl += "    FragColor = FragmentShaderBusiness();\n";
    glsl += "}\n";

    return glsl;
}

struct HelperAliasGroup
{
    const char *canonical;
    const char *aliases[4];
};

static constexpr HelperAliasGroup FS_HELPER_ALIAS_GROUPS[] = {
    {"TransformNormal", {"TransformNormal", nullptr, nullptr, nullptr}},
    {"GetWorldPos", {"GetWorldPos", "GetWorldPosition", nullptr, nullptr}},
    {"GetCameraPos", {"GetCameraPos", "GetCameraPosition", nullptr, nullptr}},
};

static bool ContainsCallToken(const char *code, const char *func_name)
{
    if (!code || !*code || !func_name || !*func_name)
        return false;

    char token[160];
    std::snprintf(token, sizeof(token), "%s(", func_name);
    return std::strstr(code, token) != nullptr;
}

static bool ContainsAnyAliasCall(const char *code, const HelperAliasGroup &group)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        const char *alias = group.aliases[i];
        if (!alias)
            break;

        if (ContainsCallToken(code, alias))
            return true;
    }

    return false;
}

static bool ContainsHelperInFSMain(const std::string &fs_main_without_business, const HelperAliasGroup &group)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        const char *alias = group.aliases[i];
        if (!alias)
            break;

        const std::string token = std::string(alias) + "(";
        if (fs_main_without_business.find(token) != std::string::npos)
            return true;
    }

    return false;
}

static void AppendUniqueHelperName(std::vector<std::string> &out, const std::string &name)
{
    for (const auto &existing : out)
    {
        if (existing == name)
            return;
    }

    out.emplace_back(name);
}

static bool IsStrictHelperConflictModeEnabled()
{
    const char *env = std::getenv("ULRE_HELPER_CONFLICT_STRICT");
    if (!env)
        return false;

    return (std::strcmp(env, "1") == 0)
        || (std::strcmp(env, "true") == 0)
        || (std::strcmp(env, "TRUE") == 0)
        || (std::strcmp(env, "on") == 0)
        || (std::strcmp(env, "ON") == 0);
}

static void AppendHelperConflictsFromDiagnostics(
    const ShaderComposeDiagnostics &diagnostics,
    std::vector<std::string> &out)
{
    if (diagnostics.helper_conflicts.empty())
    {
        if (diagnostics.helper_conflict_detected)
            AppendUniqueHelperName(out, "<conflict-detected-without-detail>");
        return;
    }

    for (const auto &item : diagnostics.helper_conflicts)
    {
        if (!item.empty())
            AppendUniqueHelperName(out, item);
    }
}

static void PrintComposedBusinessDiagnosticsJson(
    const ComposedMaterialDef &def,
    const std::vector<std::string> &helper_conflicts)
{
    if (helper_conflicts.empty())
        return;

    const char *mat_name = (def.name && def.name[0]) ? def.name : "<unnamed-material>";
    std::fprintf(stderr,
        "[ComposedBusiness][Diagnostics] {\"material\":\"%s\",\"helper_conflict_detected\":true,\"helper_conflict_count\":%u,\"helper_conflicts\":[",
        mat_name,
        static_cast<unsigned>(helper_conflicts.size()));

    for (size_t i = 0; i < helper_conflicts.size(); ++i)
    {
        std::fprintf(stderr, "%s\"%s\"", i == 0 ? "" : ",", helper_conflicts[i].c_str());
    }

    std::fprintf(stderr, "]}\n");
}

bool ValidateFSMainBusinessHelperConsistency(
    const ComposedMaterialDef &def,
    const AnsiString &generated_fs)
{
    const char *business_code = (def.fragment_business && def.fragment_business->code)
        ? def.fragment_business->code
        : nullptr;

    if (!business_code || !*business_code)
        return true;

    std::string fs_main_only = generated_fs.c_str() ? generated_fs.c_str() : "";
    const std::string business_block = business_code;
    const size_t pos = fs_main_only.find(business_block);
    if (pos != std::string::npos)
        fs_main_only.erase(pos, business_block.size());

    std::vector<std::string> missing_helpers;
    auto IsFrameworkMIHelper = [](const char *name) -> bool
    {
        if (!name || !*name)
            return false;

        return std::strcmp(name, "GetMI") == 0
            || std::strcmp(name, "GetMaterialInstance") == 0;
    };
    for (const auto &group : FS_HELPER_ALIAS_GROUPS)
    {
        if (!ContainsAnyAliasCall(business_code, group))
            continue;

        if (IsFrameworkMIHelper(group.canonical))
            continue;

        if (!ContainsHelperInFSMain(fs_main_only, group))
            AppendUniqueHelperName(missing_helpers, group.canonical);
    }

    for (const auto &declared_helper : def.logic_required_helpers)
    {
        if (declared_helper.empty())
            continue;

        if (!ContainsCallToken(business_code, declared_helper.c_str()))
            continue;

        if (IsFrameworkMIHelper(declared_helper.c_str()))
            continue;

        const std::string token = declared_helper + "(";
        if (fs_main_only.find(token) == std::string::npos)
            AppendUniqueHelperName(missing_helpers, declared_helper);
    }

    if (def.fragment_required_helpers && def.fragment_required_helper_count > 0)
    {
        for (uint32_t i = 0; i < def.fragment_required_helper_count; ++i)
        {
            const char *declared = def.fragment_required_helpers[i];
            if (!declared || !*declared)
                continue;

            if (!ContainsCallToken(business_code, declared))
                continue;

            if (IsFrameworkMIHelper(declared))
                continue;

            const std::string token = std::string(declared) + "(";
            if (fs_main_only.find(token) == std::string::npos)
                AppendUniqueHelperName(missing_helpers, declared);
        }
    }

    if (missing_helpers.empty())
        return true;

    const char *mat_name = (def.name && def.name[0]) ? def.name : "<unnamed-material>";
    for (const auto &name : missing_helpers)
    {
        std::fprintf(stderr,
            "[ComposedBusiness][HelperConsistency] material=%s missing helper '%s' in fs_main (required by FragmentShaderBusiness)\n",
            mat_name,
            name.c_str());
    }

    return false;
}

/**
 * 编译一个 FixedMaterialDef 排列到 MaterialCreateInfo。
 *
 * 内部流程：
 *   1. 若 def 中 descriptor_entries 为空，不添加任何 UBO/SSBO/Texture
 *   2. 从 FixedVertexEntry[] 设置顶点输入
 *   3. 生成排列宏前缀 + GLSL 源码，编译到 SPV
 *   4. 若 def.mi_glsl_codes 非空，设置 MaterialInstance
 */
MaterialCreateInfo *CompileFixedMaterial(
    const VulkanDevAttr *       dev_attr,
    const FixedMaterialDef &    def,
    const ShaderPermutationKey &key,
    const Material3DCreateConfig *config)
{
    if (!dev_attr)
        return nullptr;

    // ─────────────────────────────────────────────────────────────────────────
    // Step 1: 创建 MaterialCreateConfig（运行时配置优先于定义默认值）
    // ─────────────────────────────────────────────────────────────────────────

    Material3DCreateConfig cfg = config ? *config : Material3DCreateConfig();
    cfg.prim = config ? config->prim : def.primitive_type;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    if (def.geom_glsl)
        cfg.shader_stage_flag_bit |= uint32_t(ShaderStage::Geometry);

    const bool infer_has_camera = HasDescriptorNamed(def, "camera") || HasDescriptorNamed(def, "CameraInfo");
    const bool infer_has_sky = HasDescriptorNamed(def, "sky") || HasDescriptorNamed(def, "SkyInfo");
    const bool infer_has_l2w = HasDescriptorNamed(def, "l2w") || HasDescriptorNamed(def, "LocalToWorldData");
    const bool infer_has_mi = HasDescriptorNamed(def, "mtl")
                           || HasDescriptorNamed(def, "MaterialInstanceData")
                           || HasPerMaterialDescriptor(def)
                           || HasVertexEntry(def, Assign::MaterialInstanceID::VIS_NAME)
                           || (def.mi_glsl_codes && def.mi_struct_bytes > 0);

    cfg.camera = cfg.camera || infer_has_camera;
    cfg.sky = cfg.sky || infer_has_sky;
    cfg.local_to_world = cfg.local_to_world || infer_has_l2w;
    cfg.material_instance = cfg.material_instance || infer_has_mi;

    // ─────────────────────────────────────────────────────────────────────────
    // Step 2: 创建 MaterialCreateInfo
    // ─────────────────────────────────────────────────────────────────────────

    MaterialCreateInfo *mci = new MaterialCreateInfo(&cfg);
    mci->SetDevice(dev_attr);

    bool has_camera_descriptor = false;
    bool has_local_to_world_descriptor = false;
    uint32_t mi_stage_bits = uint32_t(ShaderStage::Vertex);

    // ─────────────────────────────────────────────────────────────────────────
    // Step 3: 从 FixedDescriptorEntry[] 添加描述符
    // ─────────────────────────────────────────────────────────────────────────

    for (uint32_t i = 0; i < def.descriptor_entry_count; ++i)
    {
        const FixedDescriptorEntry &entry = def.descriptor_entries[i];
        const uint32_t stage_bits = entry.stage_flags;
        const DescriptorSetType set_type = entry.set_type;

        switch (entry.kind)
        {
        case DescriptorKind::UBO:
            if (entry.struct_name)
            {
                if (std::strcmp(entry.struct_name, SBS_ViewportInfo.struct_name) == 0)
                {
                    mci->AddUBOStruct(stage_bits, SBS_ViewportInfo);
                    break;
                }

                if (std::strcmp(entry.struct_name, SBS_CameraInfo.struct_name) == 0)
                {
                    mci->AddUBOStruct(stage_bits, SBS_CameraInfo);
                    has_camera_descriptor = true;
                    break;
                }

                if (std::strcmp(entry.struct_name, SBS_SkyInfo.struct_name) == 0)
                {
                    mci->AddUBOStruct(stage_bits, SBS_SkyInfo);
                    break;
                }

                if (std::strcmp(entry.struct_name, SBS_LocalToWorld.struct_name) == 0)
                {
                    mci->SetLocalToWorld(stage_bits);
                    has_local_to_world_descriptor = true;
                    break;
                }

                if (std::strcmp(entry.struct_name, SBS_MaterialInstance.struct_name) == 0)
                {
                    mi_stage_bits = stage_bits;
                    break;
                }

                if (const ShaderBufferSource *sbs = ResolveShaderBufferSourceByStructName(&cfg, entry.struct_name))
                {
                    if (entry.set_type != sbs->set_type)
                    {
                        delete mci;
                        return nullptr;
                    }

                    if (!mci->AddUBOStruct(stage_bits, *sbs))
                    {
                        delete mci;
                        return nullptr;
                    }
                    break;
                }

                // 自定义结构体（要求 entry.struct_name 在其它地方已注册完整代码）
                if (!mci->AddUBO(stage_bits, set_type,
                                 AnsiString(entry.struct_name), AnsiString(entry.name)))
                {
                    delete mci;
                    return nullptr;
                }
            }
            break;

        case DescriptorKind::SSBO:
            if (entry.struct_name)
            {
                if (std::strcmp(entry.struct_name, SBS_LocalToWorld.struct_name) == 0)
                {
                    mci->SetLocalToWorld(stage_bits);
                    has_local_to_world_descriptor = true;
                    break;
                }

                if (std::strcmp(entry.struct_name, SBS_MaterialInstance.struct_name) == 0)
                {
                    mi_stage_bits = stage_bits;
                    break;
                }

                if (const ShaderBufferSource *sbs = ResolveShaderBufferSourceByStructName(&cfg, entry.struct_name))
                {
                    if (entry.set_type != sbs->set_type)
                    {
                        delete mci;
                        return nullptr;
                    }

                    if (!mci->AddSSBOStruct(stage_bits, *sbs))
                    {
                        delete mci;
                        return nullptr;
                    }
                    break;
                }

                if (!mci->AddSSBO(stage_bits, set_type,
                                  AnsiString(entry.struct_name), AnsiString(entry.name)))
                {
                    delete mci;
                    return nullptr;
                }
            }
            break;

        case DescriptorKind::Texture:
            // Texture 不需要 struct_name
            if (entry.glsl_type)
            {
                TextureType tt;
                const char *glsl_type_str = entry.glsl_type;
                
                // 常见类型映射
                if (strcmp(glsl_type_str, "sampler2D") == 0)
                    tt = TextureType::Texture2D;
                else if (strcmp(glsl_type_str, "sampler3D") == 0)
                    tt = TextureType::Texture3D;
                else if (strcmp(glsl_type_str, "samplerCube") == 0)
                    tt = TextureType::TextureCube;
                else if (strcmp(glsl_type_str, "sampler2DArray") == 0)
                    tt = TextureType::Texture2DArray;
                else
                    tt = TextureType::Texture2D;  // fallback

                mci->AddTexture(ShaderStage(stage_bits), set_type, tt,
                               AnsiString(entry.name));
            }
            break;

        case DescriptorKind::TextureSampler:
            if (entry.glsl_type)
            {
                TextureType tt;
                SamplerType st = SamplerType::Sampler2D; // 默认
                const char *glsl_type_str = entry.glsl_type;

                if (strcmp(glsl_type_str, "sampler2D") == 0) {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                } else if (strcmp(glsl_type_str, "samplerCube") == 0) {
                    tt = TextureType::TextureCube;
                    st = SamplerType::SamplerCube;
                } else if (strcmp(glsl_type_str, "sampler2DArray") == 0) {
                    tt = TextureType::Texture2DArray;
                    st = SamplerType::Sampler2DArray;
                } else {
                    tt = TextureType::Texture2D;
                    st = SamplerType::Sampler2D;
                }

                mci->AddTextureSampler(ShaderStage(stage_bits), set_type, st, AnsiString(entry.name));
            }
            break;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 4: 从 FixedVertexEntry[] 添加顶点输入
    // ─────────────────────────────────────────────────────────────────────────

    ShaderCreateInfoVertex *vsc = mci->GetVS();
    const bool has_position = HasVertexEntry(def, VAN::Position);
    const bool position_is_vec2 = IsPositionVec2(def);
    const bool has_color = HasVertexEntry(def, VAN::Color);
    const bool has_normal = HasVertexEntry(def, VAN::Normal);
    const bool has_transform_id = HasVertexEntry(def, Assign::TransformID::VIS_NAME);

    if (vsc)
    {
        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const FixedVertexEntry &entry = def.vertex_entries[i];
            vsc->AddInput(entry.type,
                          entry.name,
                          entry.input_rate,
                          entry.group);
        }

        if (has_transform_id)
            vsc->AddFunction(func::MF_GetLocalToWorld_ByAssign);

        if (has_color)
            vsc->AddOutput(SVT_VEC4, "Color");

        if (has_position)
        {
            if (has_transform_id && has_camera_descriptor)
                vsc->AddFunction(position_is_vec2 ? func::GetPosition3DL2WCameraBy2D : func::GetPosition3DL2WCamera);
            else if (has_transform_id)
                vsc->AddFunction(position_is_vec2 ? func::GetPosition3DL2WBy2D : func::GetPosition3DL2W);
            else if (has_camera_descriptor)
                vsc->AddFunction(position_is_vec2 ? func::GetPosition3DCameraBy2D : func::GetPosition3DCamera);
            else
                vsc->AddFunction(position_is_vec2 ? func::GetPosition3DBy2D : func::GetPosition3D);
        }

        if (has_normal && has_transform_id && has_camera_descriptor)
            vsc->AddFunction(func::GetNormalByLocal);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 5: 设置 MaterialInstance（若有）
    // ─────────────────────────────────────────────────────────────────────────

    if (def.mi_glsl_codes && def.mi_struct_bytes > 0)
    {
        mci->SetMaterialInstance(
            AnsiString(def.mi_glsl_codes),
            def.mi_struct_bytes,
            mi_stage_bits);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 6: 编译 GLSL → SPV
    //
    // 流程：
    //   1. 生成排列宏前缀：key.AppendGLSLDefines(prefix)
    //   2. prefix + def.vert_glsl/frag_glsl → 完整 GLSL
    //   3. ShaderCreateInfo 进行 glslang 编译到 SPV
    // ─────────────────────────────────────────────────────────────────────────

    AnsiString glsl_prefix;
    key.AppendGLSLDefines(glsl_prefix);

    // 设置 shaders（这里调用 mci 的 shader 编译接口）
    ShaderCreateInfoVertex *vert = mci->GetVS();
    ShaderCreateInfoFragment *frag = mci->GetFS();

    if (frag)
    {
        frag->AddOutput(VAT_VEC4, "FragColor");
    }

    // 暂时使用 SetMain() 的方式来设置源码（不完美，但能工作）
    if (vert && def.vert_glsl)
    {
        AnsiString vert_source = glsl_prefix + def.vert_glsl;
        vert->SetMain(vert_source.c_str(), vert_source.Length());
    }

    if (frag && def.frag_glsl)
    {
        AnsiString frag_source = glsl_prefix + def.frag_glsl;
        frag->SetMain(frag_source.c_str(), frag_source.Length());
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Step 7: 调用 CreateShader() 编译到 SPV
    // ─────────────────────────────────────────────────────────────────────────

    if (!mci->CreateShader())
    {
        delete mci;
        return nullptr;
    }

    return mci;
}

MaterialCreateInfo *CompileComposedBusinessMaterial(
    const VulkanDevAttr *       dev_attr,
    const FixedMaterialDef &    base_fixed_def,
    const ComposedMaterialDef & base_composed_def,
    const MaterialLogicDef &    logic,
    const ShaderPermutationKey &key,
    const Material3DCreateConfig *config)
{
    // Phase B: 校验 MaterialLogicDef 是否符合规范约束
    if (!ValidateMaterialLogicDef(logic))
    {
        std::fprintf(stderr, "[ComposedBusiness] MaterialLogicDef validation failed, see errors above\n");
        return nullptr;
    }

    ComposedMaterialBuildFromLogicResult bridge_result;
    const bool bridge_ok = BuildComposedMaterialDefFromLogic(base_composed_def, logic, bridge_result);

    if (!bridge_ok)
    {
        std::fprintf(stderr, "[ComposedBusiness] bridge failed, missing resources: ");
        for (size_t i = 0; i < bridge_result.diagnostics.missing_resources.size(); ++i)
        {
            std::fprintf(stderr,
                         "%s%s",
                         i == 0 ? "" : ", ",
                         bridge_result.diagnostics.missing_resources[i].c_str());
        }
        std::fprintf(stderr, "\n");
        return nullptr;
    }

    AnsiString generated_vs = BuildVertexGLSLFromBusiness(base_fixed_def, bridge_result.def);
    AnsiString generated_fs = BuildFragmentGLSLFromBusiness(base_fixed_def, bridge_result.def);

    if (!ValidateFSMainBusinessHelperConsistency(bridge_result.def, generated_fs))
    {
        std::fprintf(stderr,
            "[ComposedBusiness] abort compile: fs_main and FS business required helpers are inconsistent\n");
        return nullptr;
    }

    {
        PipelineMode default_pipeline_mode;
        const ShaderComposeResult vs_diag = ComposedShaderGenerator::ComposeVertexShaderWithDiagnostics(
            bridge_result.def,
            key,
            default_pipeline_mode,
            true);
        const ShaderComposeResult fs_diag = ComposedShaderGenerator::ComposeFragmentShaderWithDiagnostics(
            bridge_result.def,
            key,
            default_pipeline_mode,
            true);

        std::vector<std::string> helper_conflicts;
        AppendHelperConflictsFromDiagnostics(vs_diag.diagnostics, helper_conflicts);
        AppendHelperConflictsFromDiagnostics(fs_diag.diagnostics, helper_conflicts);
        PrintComposedBusinessDiagnosticsJson(bridge_result.def, helper_conflicts);

        if (!helper_conflicts.empty() && IsStrictHelperConflictModeEnabled())
        {
            std::fprintf(stderr,
                "[ComposedBusiness] abort compile: helper conflict detected under strict mode\n");
            return nullptr;
        }
    }

    FixedMaterialDef runtime_def = base_fixed_def;
    runtime_def.vert_glsl = generated_vs.c_str();
    runtime_def.frag_glsl = generated_fs.c_str();

    return CompileFixedMaterial(dev_attr, runtime_def, key, config);
}

}  // namespace hgl::graph::mtl
