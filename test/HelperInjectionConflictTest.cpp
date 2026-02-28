#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderComposition_Examples.h>
#include <hgl/type/String.h>

#include <cstdio>
#include <cstring>

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
    printf("=== Helper Injection Conflict Test ===\n\n");

    static const char CUSTOM_FS_BUSINESS[] = R"(
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

    const FragmentShaderBusiness custom_fragment { CUSTOM_FS_BUSINESS };

    ComposedMaterialDef def = EX_BASIC_LIT_COMPOSED;
    def.name = "HelperConflictCustomCamera";
    def.fragment_business = &custom_fragment;
    def.enable_lighting = false;

    ShaderPermutationKey key{};
    const hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(def, key);

    const bool generated_ok = fs_code.Length() > 0;
    const bool custom_body_present = std::strstr(fs_code.c_str(), "return vec3(1.0, 2.0, 3.0)") != nullptr;

    const uint32_t get_camera_pos_defs = CountToken(fs_code, "vec3 GetCameraPos() {");
    const uint32_t get_camera_position_defs = CountToken(fs_code, "vec3 GetCameraPosition() {");

    const bool no_duplicate_get_camera_pos = (get_camera_pos_defs == 1);
    const bool no_duplicate_get_camera_position = (get_camera_position_defs == 1);

    printf("Generated FS: %s\n", generated_ok ? "PASS" : "FAIL");
    printf("Custom helper body present: %s\n", custom_body_present ? "PASS" : "FAIL");
    printf("GetCameraPos definition count == 1: %s (count=%u)\n", no_duplicate_get_camera_pos ? "PASS" : "FAIL", get_camera_pos_defs);
    printf("GetCameraPosition definition count == 1: %s (count=%u)\n", no_duplicate_get_camera_position ? "PASS" : "FAIL", get_camera_position_defs);

    const bool ok = generated_ok
                 && custom_body_present
                 && no_duplicate_get_camera_pos
                 && no_duplicate_get_camera_position;

    printf("\n=== Helper Injection Conflict Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
