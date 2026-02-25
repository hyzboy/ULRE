/// test_ComposedShaderGenerator.cpp — 验证合成着色器生成器（真实生成路径）

#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderComposition_Examples.h>
#include <hgl/graph/mtl/ShaderLogic.h>
#include <hgl/type/String.h>
#include <cstdio>
#include <cstring>
#include <unordered_set>

using namespace hgl::graph::mtl;

struct ShaderTextValidation {
    const char *description;
    const char *keyword;
    bool found = false;
};

static bool DumpShaderTextFile(const char *filename, const char *text)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        printf("  [✗] 无法写入文件: %s\n", filename);
        return false;
    }

    const size_t len = strlen(text);
    const size_t written = fwrite(text, 1, len, fp);
    fclose(fp);

    if (written != len)
    {
        printf("  [✗] 文件写入不完整: %s (%u/%u)\n", filename, (unsigned)written, (unsigned)len);
        return false;
    }

    printf("  [✓] 已输出 GLSL: %s (%u bytes)\n", filename, (unsigned)len);
    return true;
}

static bool ValidateGLSL(const hgl::AnsiString &glsl_code, ShaderTextValidation *validations, uint32_t count)
{
    printf("\n[验证生成的 GLSL 代码]\n");
    printf("代码长度：%u 字符\n\n", glsl_code.Length());

    bool all_ok = true;
    for (uint32_t i = 0; i < count; i++)
    {
        validations[i].found = (std::strstr(glsl_code.c_str(), validations[i].keyword) != nullptr);
        const char *status = validations[i].found ? "✓" : "✗";
        printf("%s %s\n", status, validations[i].description);
        if (!validations[i].found)
            all_ok = false;
    }

    return all_ok;
}

static bool ValidateNoDuplicateSetBinding(const hgl::AnsiString &glsl_code, const char *label)
{
    std::unordered_set<uint64_t> binding_set;
    bool all_ok = true;

    const char *cursor = glsl_code.c_str();
    const char *token = "layout(set=";

    printf("\n[重复绑定检查] %s\n", label);

    while ((cursor = std::strstr(cursor, token)) != nullptr)
    {
        unsigned int set_id = 0;
        unsigned int binding_id = 0;

        if (std::sscanf(cursor, "layout(set=%u, binding=%u", &set_id, &binding_id) == 2)
        {
            const uint64_t key = (uint64_t(set_id) << 32) | uint64_t(binding_id);

            if (binding_set.find(key) != binding_set.end())
            {
                printf("  [✗] 发现重复 binding: set=%u, binding=%u\n", set_id, binding_id);
                all_ok = false;
            }
            else
            {
                binding_set.insert(key);
            }
        }

        cursor += 1;
    }

    if (all_ok)
    {
        printf("  [✓] 未发现重复 set/binding\n");
    }

    return all_ok;
}

static bool ValidateStage3Helpers(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[Helper 注入检查] %s\n", label);

    ShaderTextValidation helper_checks[] = {
        {"包含 TransformNormal", "TransformNormal("},
        {"包含 GetCameraPos", "GetCameraPos("},
        {"包含 GetWorldPos", "GetWorldPos("},
    };

    return ValidateGLSL(glsl_code, helper_checks, uint32_t(sizeof(helper_checks) / sizeof(helper_checks[0])));
}

static bool ValidateHelperAliasEmission(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[Helper 别名检查] %s\n", label);

    ShaderTextValidation helper_alias_checks[] = {
        {"包含 GetWorldPosition", "GetWorldPosition("},
        {"包含 GetCameraPosition", "GetCameraPosition("},
    };

    return ValidateGLSL(glsl_code, helper_alias_checks, uint32_t(sizeof(helper_alias_checks) / sizeof(helper_alias_checks[0])));
}

static bool ValidateContainsWorldAndCameraHelpersOnly(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[显式依赖注入检查] %s\n", label);

    const bool has_world = (std::strstr(glsl_code.c_str(), "GetWorldPos(") != nullptr);
    const bool has_camera = (std::strstr(glsl_code.c_str(), "GetCameraPos(") != nullptr);
    const bool has_transform = (std::strstr(glsl_code.c_str(), "TransformNormal(") != nullptr);

    printf("%s 包含 GetWorldPos\n", has_world ? "✓" : "✗");
    printf("%s 包含 GetCameraPos\n", has_camera ? "✓" : "✗");
    printf("%s 不包含 TransformNormal\n", !has_transform ? "✓" : "✗");

    return has_world && has_camera && !has_transform;
}

static bool ValidateHelperAbsence(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[Helper 未注入检查] %s\n", label);

    ShaderTextValidation helper_checks[] = {
        {"不包含 TransformNormal", "TransformNormal("},
        {"不包含 GetCameraPos", "GetCameraPos("},
        {"不包含 GetWorldPos", "GetWorldPos("},
    };

    bool all_ok = true;
    for (uint32_t i = 0; i < uint32_t(sizeof(helper_checks) / sizeof(helper_checks[0])); ++i)
    {
        const bool found = (std::strstr(glsl_code.c_str(), helper_checks[i].keyword) != nullptr);
        const char *status = found ? "✗" : "✓";
        printf("%s %s\n", status, helper_checks[i].description);
        if (found)
            all_ok = false;
    }

    return all_ok;
}

static bool ValidateStructuredDiagnosticsClean(
    const ShaderComposeResult &vs_result,
    const ShaderComposeResult &fs_result,
    const char *label)
{
    printf("\n[结构化诊断无误报检查] %s\n", label);

    const bool ok = (!vs_result.diagnostics.normal_compression_policy_normalized)
                 && (!vs_result.diagnostics.normal_policy_normalized_vertex_input)
                 && (!fs_result.diagnostics.normal_policy_normalized_normal_map)
                 && (!fs_result.diagnostics.normal_policy_normalized_gbuffer);

    printf("%s normal_compression_policy_normalized == false\n", ok ? "✓" : "✗");
    return ok;
}

static bool ValidateStructuredDiagnosticsNormalized(
    const ShaderComposeResult &vs_result,
    const ShaderComposeResult &fs_result,
    const char *label)
{
    printf("\n[结构化诊断归一化检查] %s\n", label);

    const bool ok = vs_result.diagnostics.normal_compression_policy_normalized
                 && vs_result.diagnostics.normal_policy_normalized_vertex_input
                 && fs_result.diagnostics.normal_policy_normalized_normal_map
                 && fs_result.diagnostics.normal_policy_normalized_gbuffer;

    printf("%s normal_compression_policy_normalized == true\n", ok ? "✓" : "✗");
    return ok;
}

struct NormalCompressionCheckResult
{
    bool compression_define_ok = false;
    bool compression_decode_route_ok = false;
    bool compression_gbuffer_encode_ok = false;
    bool oct_structured_diag_clean_ok = false;
    bool spheremap_macro_ok = false;
    bool spheremap_helper_ok = false;
    bool none_normalize_ok = false;
    bool none_normalize_diag_ok = false;
    bool none_structured_diag_ok = false;
};

struct CoreRegressionCheckResult
{
    bool vs_no_dup = false;
    bool fs_no_dup = false;
    bool vs_helper_absent_ok = false;
    bool fs_helper_absent_ok = false;
    bool vs_helper_ok = false;
    bool fs_helper_ok = false;
    bool fs_helper_alias_ok = false;
    bool fs_explicit_helper_ok = false;
    bool fs_logic_helper_ok = false;
    bool bridge_missing_detect_ok = false;
    bool bridge_descriptor_filter_ok = false;
    bool bridge_logic_helper_count_ok = false;
    bool bridge_helper_inject_ok = false;
    bool forward_pervertex_vs_macro_ok = false;
    bool forward_pervertex_fs_macro_ok = false;
    bool forward_pervertex_channel_ok = false;
    bool mobile_subpass_vs_macro_ok = false;
    bool mobile_subpass_fs_route_ok = false;
};

static CoreRegressionCheckResult RunCoreRegressionChecks(
    const hgl::AnsiString &vs_code,
    const hgl::AnsiString &fs_code,
    const ShaderPermutationKey &key)
{
    CoreRegressionCheckResult out;

    out.vs_no_dup = ValidateNoDuplicateSetBinding(vs_code, "VS");
    out.fs_no_dup = ValidateNoDuplicateSetBinding(fs_code, "FS");

    out.vs_helper_absent_ok = ValidateHelperAbsence(vs_code, "VS(BasicLit)");
    out.fs_helper_absent_ok = ValidateHelperAbsence(fs_code, "FS(BasicLit)");

    static const char HELPER_DEMAND_VS_BUSINESS[] = R"(
        vec4 VertexShaderBusiness(const VertexInput vi) {
            return vec4(vi.Position, 1.0);
        }
    )";

    static const char HELPER_DEMAND_FS_BUSINESS[] = R"(
        vec4 FragmentShaderBusiness(const VS_Output vso) {
            vec3 world_n = TransformNormal(vso.WorldNormal);
            vec3 view_dir = normalize(GetCameraPosition() - GetWorldPosition());
            float ndv = max(dot(world_n, view_dir), 0.0);
            return vec4(vec3(ndv), 1.0);
        }
    )";

    const VertexShaderBusiness HELPER_DEMAND_VERTEX_BUSINESS { HELPER_DEMAND_VS_BUSINESS };
    const FragmentShaderBusiness HELPER_DEMAND_FRAGMENT_BUSINESS { HELPER_DEMAND_FS_BUSINESS };

    const ComposedMaterialDef HELPER_DEMAND_COMPOSED {
        .name = "HelperDemand",
        .primitive_type = EX_BASIC_LIT_COMPOSED.primitive_type,
        .vertex_entries = EX_BASIC_LIT_VERTEX,
        .vertex_entry_count = 3,
        .descriptor_entries = EX_BASIC_LIT_DESCRIPTORS,
        .descriptor_entry_count = 7,
        .vertex_business = &HELPER_DEMAND_VERTEX_BUSINESS,
        .fragment_business = &HELPER_DEMAND_FRAGMENT_BUSINESS,
        .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
        .enable_lighting = false,
        .mi_glsl_codes = EX_BASIC_LIT_MI_GLSL,
        .mi_struct_bytes = sizeof(float) * 3,
    };

    hgl::AnsiString helper_vs_code = ComposedShaderGenerator::ComposeVertexShader(HELPER_DEMAND_COMPOSED, key);
    hgl::AnsiString helper_fs_code = ComposedShaderGenerator::ComposeFragmentShader(HELPER_DEMAND_COMPOSED, key);

    out.vs_helper_ok = ValidateHelperAbsence(helper_vs_code, "VS(HelperDemand)");
    out.fs_helper_ok = ValidateStage3Helpers(helper_fs_code, "FS(HelperDemand)");
    out.fs_helper_alias_ok = ValidateHelperAliasEmission(helper_fs_code, "FS(HelperDemand)");

    static const char EXPLICIT_HELPER_FS_BUSINESS[] = R"(
        vec4 FragmentShaderBusiness(const VS_Output vso) {
            // 这里不直接调用 helper，验证显式依赖是否生效
            return vec4(vso.WorldNormal * 0.5 + vec3(0.5), 1.0);
        }
    )";

    const FragmentShaderBusiness EXPLICIT_HELPER_FRAGMENT_BUSINESS { EXPLICIT_HELPER_FS_BUSINESS };
    const char *EXPLICIT_FS_HELPERS[] = {
        "GetWorldPos",
        "GetCameraPosition",
    };

    const ComposedMaterialDef EXPLICIT_HELPER_COMPOSED {
        .name = "ExplicitHelperDemand",
        .primitive_type = EX_BASIC_LIT_COMPOSED.primitive_type,
        .vertex_entries = EX_BASIC_LIT_VERTEX,
        .vertex_entry_count = 3,
        .descriptor_entries = EX_BASIC_LIT_DESCRIPTORS,
        .descriptor_entry_count = 7,
        .vertex_business = &HELPER_DEMAND_VERTEX_BUSINESS,
        .fragment_business = &EXPLICIT_HELPER_FRAGMENT_BUSINESS,
        .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
        .enable_lighting = false,
        .mi_glsl_codes = EX_BASIC_LIT_MI_GLSL,
        .mi_struct_bytes = sizeof(float) * 3,
        .vertex_required_helpers = nullptr,
        .vertex_required_helper_count = 0,
        .fragment_required_helpers = EXPLICIT_FS_HELPERS,
        .fragment_required_helper_count = 2,
    };

    hgl::AnsiString explicit_fs_code = ComposedShaderGenerator::ComposeFragmentShader(EXPLICIT_HELPER_COMPOSED, key);
    out.fs_explicit_helper_ok = ValidateContainsWorldAndCameraHelpersOnly(explicit_fs_code, "FS(ExplicitHelperDemand)");

    const FragmentShaderBusiness LOGIC_HELPER_FRAGMENT_BUSINESS { EXPLICIT_HELPER_FS_BUSINESS };
    std::vector<std::string> LOGIC_FS_HELPERS = {"GetWorldPos", "GetCameraPosition"};

    ComposedMaterialDef LOGIC_HELPER_COMPOSED = EX_BASIC_LIT_COMPOSED;
    LOGIC_HELPER_COMPOSED.name = "LogicHelperDemand";
    LOGIC_HELPER_COMPOSED.vertex_business = &HELPER_DEMAND_VERTEX_BUSINESS;
    LOGIC_HELPER_COMPOSED.fragment_business = &LOGIC_HELPER_FRAGMENT_BUSINESS;
    LOGIC_HELPER_COMPOSED.logic_required_helpers = LOGIC_FS_HELPERS;

    hgl::AnsiString logic_fs_code = ComposedShaderGenerator::ComposeFragmentShader(LOGIC_HELPER_COMPOSED, key);
    out.fs_logic_helper_ok = ValidateContainsWorldAndCameraHelpersOnly(logic_fs_code, "FS(LogicHelperDemand)");

    const char *LOGIC_VERTEX_REQUIRED_RESOURCES[] = {
        "MaterialInstanceData",
        "LocalToWorld",
    };
    const char *LOGIC_FRAGMENT_REQUIRED_RESOURCES[] = {
        "camera",
        "ThisResourceDoesNotExist",
    };
    const char *LOGIC_VERTEX_REQUIRED_HELPERS[] = {
        "GetWorldPos",
    };
    const char *LOGIC_FRAGMENT_REQUIRED_HELPERS[] = {
        "GetCameraPos",
    };

    MaterialLogicDef bridge_logic = {};
    bridge_logic.vertex.main_logic = HELPER_DEMAND_VS_BUSINESS;
    bridge_logic.vertex.custom_functions = nullptr;
    bridge_logic.vertex.required_resources = LOGIC_VERTEX_REQUIRED_RESOURCES;
    bridge_logic.vertex.required_resource_count = 2;
    bridge_logic.vertex.required_helpers = LOGIC_VERTEX_REQUIRED_HELPERS;
    bridge_logic.vertex.required_helper_count = 1;
    bridge_logic.fragment.main_logic = EXPLICIT_HELPER_FS_BUSINESS;
    bridge_logic.fragment.custom_functions = nullptr;
    bridge_logic.fragment.required_resources = LOGIC_FRAGMENT_REQUIRED_RESOURCES;
    bridge_logic.fragment.required_resource_count = 2;
    bridge_logic.fragment.required_helpers = LOGIC_FRAGMENT_REQUIRED_HELPERS;
    bridge_logic.fragment.required_helper_count = 1;

    ComposedMaterialBuildFromLogicResult bridge_result;
    const bool bridge_ok = BuildComposedMaterialDefFromLogic(EX_BASIC_LIT_COMPOSED, bridge_logic, bridge_result);

    out.bridge_missing_detect_ok = (!bridge_ok)
                                && (bridge_result.diagnostics.missing_resources.size() == 1)
                                && (bridge_result.diagnostics.missing_resources[0] == "ThisResourceDoesNotExist");

    out.bridge_descriptor_filter_ok = (bridge_result.def.descriptor_entry_count == 3);

    out.bridge_logic_helper_count_ok = (bridge_result.def.logic_required_helpers.size() == 2);
    bool bridge_logic_helper_contains_world = false;
    bool bridge_logic_helper_contains_camera = false;
    for (const auto &name : bridge_result.def.logic_required_helpers)
    {
        if (name == "GetWorldPos")
            bridge_logic_helper_contains_world = true;
        if (name == "GetCameraPos")
            bridge_logic_helper_contains_camera = true;
    }
    out.bridge_logic_helper_count_ok = out.bridge_logic_helper_count_ok
                                    && bridge_logic_helper_contains_world
                                    && bridge_logic_helper_contains_camera;

    hgl::AnsiString bridge_fs_code = ComposedShaderGenerator::ComposeFragmentShader(bridge_result.def, key);
    out.bridge_helper_inject_ok = ValidateContainsWorldAndCameraHelpersOnly(bridge_fs_code, "FS(BridgeLogic)");

    PipelineMode forward_pervertex_mode;
    forward_pervertex_mode.render_path = PipelineRenderPath::Forward;
    forward_pervertex_mode.forward_lighting = PipelineForwardLightingMode::PerVertex;

    hgl::AnsiString forward_pervertex_vs = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key, forward_pervertex_mode);
    hgl::AnsiString forward_pervertex_fs = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key, forward_pervertex_mode);

    out.forward_pervertex_vs_macro_ok = (std::strstr(forward_pervertex_vs.c_str(), "#define FORWARD_LIGHTING_PER_VERTEX 1") != nullptr)
                                    && (std::strstr(forward_pervertex_vs.c_str(), "#define FORWARD_LIGHTING_PER_PIXEL 0") != nullptr);

    out.forward_pervertex_fs_macro_ok = (std::strstr(forward_pervertex_fs.c_str(), "#define FORWARD_LIGHTING_PER_VERTEX 1") != nullptr)
                                    && (std::strstr(forward_pervertex_fs.c_str(), "#define FORWARD_LIGHTING_PER_PIXEL 0") != nullptr);

    out.forward_pervertex_channel_ok = (std::strstr(forward_pervertex_vs.c_str(), "vec3 VertexLighting") != nullptr)
                                    && (std::strstr(forward_pervertex_vs.c_str(), "vso.VertexLighting = _vertex_light") != nullptr)
                                    && (std::strstr(forward_pervertex_fs.c_str(), "business_output.rgb *= clamp(vso.VertexLighting") != nullptr);

    PipelineMode mobile_subpass_mode;
    mobile_subpass_mode.render_path = PipelineRenderPath::MobileSubpassGBufferDeferred;

    hgl::AnsiString mobile_subpass_vs = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key, mobile_subpass_mode);
    hgl::AnsiString mobile_subpass_fs = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key, mobile_subpass_mode);

    out.mobile_subpass_vs_macro_ok = (std::strstr(mobile_subpass_vs.c_str(), "#define MOBILE_SUBPASS_GBUFFER 1") != nullptr)
                                  && (std::strstr(mobile_subpass_vs.c_str(), "#define MOBILE_SUBPASS_USE_SUBPASSLOAD 1") != nullptr);

    out.mobile_subpass_fs_route_ok = (std::strstr(mobile_subpass_fs.c_str(), "#define MOBILE_SUBPASS_GBUFFER 1") != nullptr)
                                  && (std::strstr(mobile_subpass_fs.c_str(), "subpassLoad(") != nullptr)
                                  && (std::strstr(mobile_subpass_fs.c_str(), "MobileSubpassGBufferDeferred route") != nullptr);

    return out;
}

static NormalCompressionCheckResult RunNormalCompressionChecks(const ShaderPermutationKey &key)
{
    NormalCompressionCheckResult out;

    PipelineMode normal_compression_mode;
    normal_compression_mode.render_path = PipelineRenderPath::Forward;
    normal_compression_mode.normal_compression.compress_vertex_input_normal = true;
    normal_compression_mode.normal_compression.compress_normal_map = true;
    normal_compression_mode.normal_compression.compress_gbuffer_normal = true;
    normal_compression_mode.normal_compression.vertex_input_encoding = NormalEncodingMode::Octahedral;
    normal_compression_mode.normal_compression.normal_map_encoding = NormalEncodingMode::Octahedral;
    normal_compression_mode.normal_compression.gbuffer_encoding = NormalEncodingMode::Octahedral;

    hgl::AnsiString normal_vs = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key, normal_compression_mode);
    hgl::AnsiString normal_fs = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key, normal_compression_mode);

    ComposedMaterialDef deferred_composed = EX_BASIC_LIT_COMPOSED;
    deferred_composed.output_mode = ShaderOutputMode::DualRTDeferred;
    hgl::AnsiString normal_fs_deferred = ComposedShaderGenerator::ComposeFragmentShader(deferred_composed, key, normal_compression_mode);

    out.compression_define_ok = (std::strstr(normal_vs.c_str(), "#define COMPRESS_VERTEX_INPUT_NORMAL 1") != nullptr)
                             && (std::strstr(normal_vs.c_str(), "#define COMPRESS_NORMAL_MAP 1") != nullptr)
                             && (std::strstr(normal_vs.c_str(), "#define COMPRESS_GBUFFER_NORMAL 1") != nullptr);

    out.compression_decode_route_ok = (std::strstr(normal_vs.c_str(), "DecodeVertexInputNormal(Normal)") != nullptr)
                                   && (std::strstr(normal_fs.c_str(), "DecodeMaterialNormal(") != nullptr);

    out.compression_gbuffer_encode_ok = (std::strstr(normal_fs_deferred.c_str(), "EncodeGBufferNormal(GetWorldNormal())") != nullptr);

    ShaderComposeResult normal_oct_vs_with_diag = ComposedShaderGenerator::ComposeVertexShaderWithDiagnostics(EX_BASIC_LIT_COMPOSED, key, normal_compression_mode);
    ShaderComposeResult normal_oct_fs_with_diag = ComposedShaderGenerator::ComposeFragmentShaderWithDiagnostics(EX_BASIC_LIT_COMPOSED, key, normal_compression_mode);

    out.oct_structured_diag_clean_ok = ValidateStructuredDiagnosticsClean(
        normal_oct_vs_with_diag,
        normal_oct_fs_with_diag,
        "NormalOct");

    PipelineMode normal_spheremap_mode = normal_compression_mode;
    normal_spheremap_mode.normal_compression.vertex_input_encoding = NormalEncodingMode::Spheremap;
    normal_spheremap_mode.normal_compression.normal_map_encoding = NormalEncodingMode::Spheremap;
    normal_spheremap_mode.normal_compression.gbuffer_encoding = NormalEncodingMode::Spheremap;

    hgl::AnsiString normal_spheremap_vs = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key, normal_spheremap_mode);
    hgl::AnsiString normal_spheremap_fs = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key, normal_spheremap_mode);

    out.spheremap_macro_ok = (std::strstr(normal_spheremap_vs.c_str(), "#define VERTEX_NORMAL_ENCODING_SPHEREMAP 1") != nullptr)
                          && (std::strstr(normal_spheremap_fs.c_str(), "#define NORMAL_MAP_ENCODING_SPHEREMAP 1") != nullptr)
                          && (std::strstr(normal_spheremap_fs.c_str(), "#define GBUFFER_NORMAL_ENCODING_SPHEREMAP 1") != nullptr);

    out.spheremap_helper_ok = (std::strstr(normal_spheremap_vs.c_str(), "DecodeNormalSpheremap(normal_in.xy)") != nullptr)
                           && (std::strstr(normal_spheremap_fs.c_str(), "DecodeNormalSpheremap(normal_sample.xy)") != nullptr)
                           && (std::strstr(normal_spheremap_fs.c_str(), "EncodeNormalSpheremap(n)") != nullptr);

    PipelineMode normal_none_mode = normal_compression_mode;
    normal_none_mode.normal_compression.vertex_input_encoding = NormalEncodingMode::None;
    normal_none_mode.normal_compression.normal_map_encoding = NormalEncodingMode::None;
    normal_none_mode.normal_compression.gbuffer_encoding = NormalEncodingMode::None;

    hgl::AnsiString normal_none_vs = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key, normal_none_mode);
    hgl::AnsiString normal_none_fs = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key, normal_none_mode);
    ShaderComposeResult normal_none_vs_with_diag = ComposedShaderGenerator::ComposeVertexShaderWithDiagnostics(EX_BASIC_LIT_COMPOSED, key, normal_none_mode);
    ShaderComposeResult normal_none_fs_with_diag = ComposedShaderGenerator::ComposeFragmentShaderWithDiagnostics(EX_BASIC_LIT_COMPOSED, key, normal_none_mode);

    out.none_normalize_ok = (std::strstr(normal_none_vs.c_str(), "#define COMPRESS_VERTEX_INPUT_NORMAL 0") != nullptr)
                         && (std::strstr(normal_none_fs.c_str(), "#define COMPRESS_NORMAL_MAP 0") != nullptr)
                         && (std::strstr(normal_none_fs.c_str(), "#define COMPRESS_GBUFFER_NORMAL 0") != nullptr)
                         && (std::strstr(normal_none_vs.c_str(), "#define VERTEX_NORMAL_ENCODING_NONE 1") != nullptr)
                         && (std::strstr(normal_none_fs.c_str(), "#define NORMAL_MAP_ENCODING_NONE 1") != nullptr)
                         && (std::strstr(normal_none_fs.c_str(), "#define GBUFFER_NORMAL_ENCODING_NONE 1") != nullptr);

    out.none_normalize_diag_ok = (std::strstr(normal_none_vs.c_str(), "// NORMAL_COMPRESSION_POLICY_NORMALIZED") != nullptr)
                              && (std::strstr(normal_none_vs.c_str(), "// NORMAL_POLICY_NORMALIZED_VERTEX_INPUT=1") != nullptr)
                              && (std::strstr(normal_none_fs.c_str(), "// NORMAL_POLICY_NORMALIZED_NORMAL_MAP=1") != nullptr)
                              && (std::strstr(normal_none_fs.c_str(), "// NORMAL_POLICY_NORMALIZED_GBUFFER=1") != nullptr);

    out.none_structured_diag_ok = ValidateStructuredDiagnosticsNormalized(
        normal_none_vs_with_diag,
        normal_none_fs_with_diag,
        "NormalNone");

    return out;
}

int main()
{
    printf("========================================\n");
    printf("  ComposedShaderGenerator 验证测试\n");
    printf("========================================\n\n");

    printf("[Step 1] 生成真实 VS/FS GLSL（通过公开 API）\n");

    ShaderPermutationKey key{};

    hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key);
    hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key);

    printf("  VS 长度: %u\n", vs_code.Length());
    printf("  FS 长度: %u\n", fs_code.Length());

    printf("\n[Step 2] 导出 GLSL 文件（失败时用于排查）\n");

    bool dump_vs_ok = DumpShaderTextFile("generated_vs.glsl", vs_code.c_str());
    bool dump_fs_ok = DumpShaderTextFile("generated_fs.glsl", fs_code.c_str());

    printf("\n[Step 3] 关键结构检查\n");

    ShaderTextValidation vs_validations[] = {
        {"包含 #version", "#version"},
        {"包含 VertexInput", "struct VertexInput"},
        {"包含 VS_Output", "struct VS_Output"},
        {"包含 GetLocalToWorld", "GetLocalToWorld"},
        {"包含 GetNormal", "GetNormal"},
        {"包含 main", "void main()"},
    };

    ShaderTextValidation fs_validations[] = {
        {"包含 #version", "#version"},
        {"包含 VS_Output", "struct VS_Output"},
        {"包含 ComposeFinalOutput", "ComposeFinalOutput"},
        {"包含 FragmentShaderBusiness", "FragmentShaderBusiness"},
        {"包含 main", "void main()"},
    };

    bool vs_ok = ValidateGLSL(vs_code, vs_validations, uint32_t(sizeof(vs_validations) / sizeof(vs_validations[0])));
    bool fs_ok = ValidateGLSL(fs_code, fs_validations, uint32_t(sizeof(fs_validations) / sizeof(fs_validations[0])));

    printf("\n[Step 4] Stage 2/3 回归检查\n");

    const CoreRegressionCheckResult core_checks = RunCoreRegressionChecks(vs_code, fs_code, key);

    const NormalCompressionCheckResult normal_checks = RunNormalCompressionChecks(key);

    printf("\n========================================\n");
    printf("  测试总结\n");
    printf("========================================\n");
    printf("  VS 生成:   %s\n", vs_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS 生成:   %s\n", fs_ok ? "✓ 通过" : "✗ 失败");
    printf("  VS 无重复绑定: %s\n", core_checks.vs_no_dup ? "✓ 通过" : "✗ 失败");
    printf("  FS 无重复绑定: %s\n", core_checks.fs_no_dup ? "✓ 通过" : "✗ 失败");
    printf("  VS Helper 未注入(BasicLit): %s\n", core_checks.vs_helper_absent_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS Helper 未注入(BasicLit): %s\n", core_checks.fs_helper_absent_ok ? "✓ 通过" : "✗ 失败");
    printf("  VS Helper 未注入(HelperDemand): %s\n", core_checks.vs_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS Helper 注入(HelperDemand): %s\n", core_checks.fs_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS Helper 别名输出(HelperDemand): %s\n", core_checks.fs_helper_alias_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS 显式依赖注入(ExplicitHelperDemand): %s\n", core_checks.fs_explicit_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS 逻辑依赖注入(LogicHelperDemand): %s\n", core_checks.fs_logic_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  逻辑桥接缺失资源诊断: %s\n", core_checks.bridge_missing_detect_ok ? "✓ 通过" : "✗ 失败");
    printf("  逻辑桥接描述符过滤: %s\n", core_checks.bridge_descriptor_filter_ok ? "✓ 通过" : "✗ 失败");
    printf("  逻辑桥接Helper聚合: %s\n", core_checks.bridge_logic_helper_count_ok ? "✓ 通过" : "✗ 失败");
    printf("  逻辑桥接Helper注入: %s\n", core_checks.bridge_helper_inject_ok ? "✓ 通过" : "✗ 失败");
    printf("  Forward PerVertex VS宏路由: %s\n", core_checks.forward_pervertex_vs_macro_ok ? "✓ 通过" : "✗ 失败");
    printf("  Forward PerVertex FS宏路由: %s\n", core_checks.forward_pervertex_fs_macro_ok ? "✓ 通过" : "✗ 失败");
    printf("  Forward PerVertex 通道连通: %s\n", core_checks.forward_pervertex_channel_ok ? "✓ 通过" : "✗ 失败");
    printf("  MobileSubpass VS宏路由: %s\n", core_checks.mobile_subpass_vs_macro_ok ? "✓ 通过" : "✗ 失败");
    printf("  MobileSubpass FS子通道路由: %s\n", core_checks.mobile_subpass_fs_route_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal压缩 宏注入: %s\n", normal_checks.compression_define_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal压缩 解码路由: %s\n", normal_checks.compression_decode_route_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal压缩 GBuffer编码路由: %s\n", normal_checks.compression_gbuffer_encode_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal Oct 结构化诊断无误报: %s\n", normal_checks.oct_structured_diag_clean_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal Spheremap 宏路由: %s\n", normal_checks.spheremap_macro_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal Spheremap Helper路由: %s\n", normal_checks.spheremap_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal None 组合归一化: %s\n", normal_checks.none_normalize_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal None 归一化诊断: %s\n", normal_checks.none_normalize_diag_ok ? "✓ 通过" : "✗ 失败");
    printf("  Normal None 结构化诊断: %s\n", normal_checks.none_structured_diag_ok ? "✓ 通过" : "✗ 失败");
    printf("  GLSL 导出: %s\n", (dump_vs_ok && dump_fs_ok) ? "✓ 成功" : "✗ 失败");

    const bool all_ok = vs_ok && fs_ok && core_checks.vs_no_dup && core_checks.fs_no_dup
                     && core_checks.vs_helper_absent_ok && core_checks.fs_helper_absent_ok
                     && core_checks.vs_helper_ok && core_checks.fs_helper_ok
                     && core_checks.fs_helper_alias_ok
                     && core_checks.fs_explicit_helper_ok
                     && core_checks.fs_logic_helper_ok
                     && core_checks.bridge_missing_detect_ok
                     && core_checks.bridge_descriptor_filter_ok
                     && core_checks.bridge_logic_helper_count_ok
                     && core_checks.bridge_helper_inject_ok
                     && core_checks.forward_pervertex_vs_macro_ok
                     && core_checks.forward_pervertex_fs_macro_ok
                     && core_checks.forward_pervertex_channel_ok
                     && core_checks.mobile_subpass_vs_macro_ok
                     && core_checks.mobile_subpass_fs_route_ok
                     && normal_checks.compression_define_ok
                     && normal_checks.compression_decode_route_ok
                     && normal_checks.compression_gbuffer_encode_ok
                     && normal_checks.oct_structured_diag_clean_ok
                     && normal_checks.spheremap_macro_ok
                     && normal_checks.spheremap_helper_ok
                     && normal_checks.none_normalize_ok
                     && normal_checks.none_normalize_diag_ok
                     && normal_checks.none_structured_diag_ok
                     && dump_vs_ok && dump_fs_ok;
    printf("  总体结果: %s\n\n", all_ok ? "✓✓✓ 全部通过" : "✗✗✗ 存在失败");

    if (!all_ok)
    {
        printf("请查看 generated_vs.glsl / generated_fs.glsl 进行定位。\n");
        return 1;
    }

    return 0;
}
