#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderComposition_Examples.h>

#include <cstdio>

using namespace hgl::graph::mtl;

int main()
{
    printf("=== Composed Diagnostics Aggregation Test ===\n\n");

    static const char FS_CUSTOM_BUSINESS[] = R"(
vec3 GetCameraPos() {
    return vec3(1.0, 2.0, 3.0);
}

vec3 GetCameraPosition() {
    return GetCameraPos();
}

vec4 FragmentShaderBusiness(const VS_Output vso) {
    vec3 cam = GetCameraPos();
    return vec4(cam * 0.0 + vec3(1.0), 1.0);
}
)";

    ShaderPermutationKey key{};
    PipelineMode default_pipeline_mode;

    const FragmentShaderBusiness fs_custom { FS_CUSTOM_BUSINESS };
    ComposedMaterialDef def = EX_BASIC_LIT_COMPOSED;
    def.name = "ComposedDiagnosticsAggregation";
    def.fragment_business = &fs_custom;
    def.enable_lighting = false;

    const ShaderComposeResult result = ComposedShaderGenerator::ComposeFragmentShaderWithDiagnostics(
        def,
        key,
        default_pipeline_mode,
        true);

    const bool has_code = result.code.Length() > 0;
    const bool conflict_detected = result.diagnostics.helper_conflict_detected;
    const bool conflict_count_ok = result.diagnostics.helper_conflict_count >= 1;

    printf(
        "[ComposedBusiness][Diagnostics] {\"material\":\"%s\",\"stage\":\"fragment\",\"helper_conflict_detected\":%s,\"helper_conflict_count\":%u}\n",
        def.name,
        conflict_detected ? "true" : "false",
        result.diagnostics.helper_conflict_count);

    printf("Generated code: %s\n", has_code ? "PASS" : "FAIL");
    printf("Diagnostics helper_conflict_detected: %s\n", conflict_detected ? "PASS" : "FAIL");
    printf("Diagnostics helper_conflict_count>=1: %s\n", conflict_count_ok ? "PASS" : "FAIL");

    const bool ok = has_code && conflict_detected && conflict_count_ok;
    printf("\n=== Composed Diagnostics Aggregation Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
