#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderComposition_Examples.h>
#include <hgl/type/String.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>

using namespace hgl::graph::mtl;

static uint32_t CountToken(const hgl::AnsiString &text, const char *token)
{
    if (!token || !*token)
        return 0;

    const char *cursor = text.c_str();
    uint32_t count = 0;
    while ((cursor = std::strstr(cursor, token)) != nullptr)
    {
        ++count;
        ++cursor;
    }
    return count;
}

int main()
{
    printf("=== Helper Injection Conflict Matrix Test ===\n\n");

    ShaderPermutationKey key{};

    // Case 1: FS conflict (custom GetCameraPos/GetCameraPosition)
    static const char FS_CUSTOM_BUSINESS[] = R"(
vec3 GetCameraPos() { return vec3(1.0, 2.0, 3.0); }
vec3 GetCameraPosition() { return GetCameraPos(); }
vec4 FragmentShaderBusiness(const VS_Output vso) {
    vec3 cam = GetCameraPos();
    return vec4(cam * 0.0 + vec3(1.0), 1.0);
}
)";

    const FragmentShaderBusiness fs_custom { FS_CUSTOM_BUSINESS };
    ComposedMaterialDef fs_def = EX_BASIC_LIT_COMPOSED;
    fs_def.name = "HelperConflictMatrixFS";
    fs_def.fragment_business = &fs_custom;
    fs_def.enable_lighting = false;

    PipelineMode default_pipeline_mode;

    const hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(fs_def, key);
    const ShaderComposeResult fs_diag_result = ComposedShaderGenerator::ComposeFragmentShaderWithDiagnostics(
        fs_def,
        key,
        default_pipeline_mode,
        true);

    const bool fs_conflict_marker = (std::strstr(fs_code.c_str(), "// HELPER_CONFLICT:") != nullptr)
                                 && (std::strstr(fs_code.c_str(), "helper=GetCameraPos") != nullptr);
    const bool fs_no_dup_get_camera_pos = (CountToken(fs_code, "vec3 GetCameraPos()") == 1);
    const bool fs_no_dup_get_camera_position = (CountToken(fs_code, "vec3 GetCameraPosition()") == 1);
    const bool fs_diag_conflict_detected = fs_diag_result.diagnostics.helper_conflict_detected;
    const bool fs_diag_conflict_count_ok = fs_diag_result.diagnostics.helper_conflict_count >= 1;
    const bool fs_diag_conflict_detail_ok = !fs_diag_result.diagnostics.helper_conflicts.empty();

    printf("[Case 1][FS] conflict marker: %s\n", fs_conflict_marker ? "PASS" : "FAIL");
    printf("[Case 1][FS] GetCameraPos no duplicate: %s\n", fs_no_dup_get_camera_pos ? "PASS" : "FAIL");
    printf("[Case 1][FS] GetCameraPosition no duplicate: %s\n", fs_no_dup_get_camera_position ? "PASS" : "FAIL");
    printf("[Case 1][FS] diagnostics conflict_detected: %s\n", fs_diag_conflict_detected ? "PASS" : "FAIL");
    printf("[Case 1][FS] diagnostics conflict_count>=1: %s\n", fs_diag_conflict_count_ok ? "PASS" : "FAIL");
    printf("[Case 1][FS] diagnostics conflict_detail non-empty: %s\n", fs_diag_conflict_detail_ok ? "PASS" : "FAIL");

    // Case 2: VS conflict (custom TransformNormal)
    static const char VS_CUSTOM_BUSINESS[] = R"(
vec3 TransformNormal(vec3 local_normal) {
    return normalize(local_normal);
}

vec4 VertexShaderBusiness(const VertexInput vi) {
    Output.WorldNormal = TransformNormal(vi.Normal);
    return vec4(vi.Position, 1.0);
}
)";

    const VertexShaderBusiness vs_custom { VS_CUSTOM_BUSINESS };
    ComposedMaterialDef vs_def = EX_BASIC_LIT_COMPOSED;
    vs_def.name = "HelperConflictMatrixVS";
    vs_def.vertex_business = &vs_custom;

    const hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(vs_def, key);
    const ShaderComposeResult vs_diag_result = ComposedShaderGenerator::ComposeVertexShaderWithDiagnostics(
        vs_def,
        key,
        default_pipeline_mode,
        true);

    const bool vs_conflict_marker = (std::strstr(vs_code.c_str(), "// HELPER_CONFLICT:") != nullptr)
                                 && (std::strstr(vs_code.c_str(), "helper=TransformNormal") != nullptr);
    const bool vs_no_dup_transform_normal = (CountToken(vs_code, "vec3 TransformNormal(") == 1);
    const bool vs_diag_conflict_detected = vs_diag_result.diagnostics.helper_conflict_detected;
    const bool vs_diag_conflict_count_ok = vs_diag_result.diagnostics.helper_conflict_count >= 1;
    const bool vs_diag_conflict_detail_ok = !vs_diag_result.diagnostics.helper_conflicts.empty();

    printf("[Case 2][VS] conflict marker: %s\n", vs_conflict_marker ? "PASS" : "FAIL");
    printf("[Case 2][VS] TransformNormal no duplicate: %s\n", vs_no_dup_transform_normal ? "PASS" : "FAIL");
    printf("[Case 2][VS] diagnostics conflict_detected: %s\n", vs_diag_conflict_detected ? "PASS" : "FAIL");
    printf("[Case 2][VS] diagnostics conflict_count>=1: %s\n", vs_diag_conflict_count_ok ? "PASS" : "FAIL");
    printf("[Case 2][VS] diagnostics conflict_detail non-empty: %s\n", vs_diag_conflict_detail_ok ? "PASS" : "FAIL");

    // Case 3: strict mode injects #error marker
#ifdef _WIN32
    _putenv_s("ULRE_HELPER_CONFLICT_STRICT", "1");
#else
    setenv("ULRE_HELPER_CONFLICT_STRICT", "1", 1);
#endif

    const hgl::AnsiString strict_fs_code = ComposedShaderGenerator::ComposeFragmentShader(fs_def, key);
    const bool strict_error_injected = (std::strstr(strict_fs_code.c_str(), "#error ULRE_HELPER_CONFLICT") != nullptr);

    printf("[Case 3][Strict] #error injected: %s\n", strict_error_injected ? "PASS" : "FAIL");

#ifdef _WIN32
    _putenv_s("ULRE_HELPER_CONFLICT_STRICT", "0");
#else
    setenv("ULRE_HELPER_CONFLICT_STRICT", "0", 1);
#endif

    const bool ok = fs_conflict_marker
                 && fs_no_dup_get_camera_pos
                 && fs_no_dup_get_camera_position
                 && fs_diag_conflict_detected
                 && fs_diag_conflict_count_ok
                 && fs_diag_conflict_detail_ok
                 && vs_conflict_marker
                 && vs_no_dup_transform_normal
                 && vs_diag_conflict_detected
                 && vs_diag_conflict_count_ok
                 && vs_diag_conflict_detail_ok
                 && strict_error_injected;

    printf("\n=== Helper Injection Conflict Matrix Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
