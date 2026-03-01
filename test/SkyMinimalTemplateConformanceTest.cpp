#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_SkyMinimal.h"
#include "ShaderGen/3d/S_SkyMinimal_Logic.h"

#include <cstdio>
#include <cstring>

using namespace hgl::graph::mtl;

static bool ContainsKeyword(const hgl::AnsiString &text, const char *keyword)
{
    return std::strstr(text.c_str(), keyword) != nullptr;
}

static bool EqualsString(const char *lhs, const char *rhs)
{
    if (!lhs || !rhs)
        return false;

    return std::strcmp(lhs, rhs) == 0;
}

int main()
{
    printf("=== SkyMinimal Template Conformance Test ===\n\n");

    const bool logic_valid = ValidateMaterialLogicDef(SKY_MINIMAL_LOGIC);

    const bool required_resource_shape_ok =
        SKY_MINIMAL_VERTEX_SHADER_LOGIC.required_resource_count == 2
        && SKY_MINIMAL_FRAGMENT_SHADER_LOGIC.required_resource_count == 1;

    const bool required_resource_names_ok =
        EqualsString(SKY_MINIMAL_VERTEX_SHADER_LOGIC.required_resources[0], "camera")
        && EqualsString(SKY_MINIMAL_VERTEX_SHADER_LOGIC.required_resources[1], "l2w")
        && EqualsString(SKY_MINIMAL_FRAGMENT_SHADER_LOGIC.required_resources[0], "sky");

    const bool required_helpers_ok =
        SKY_MINIMAL_VERTEX_SHADER_LOGIC.required_helper_count == 0
        && SKY_MINIMAL_FRAGMENT_SHADER_LOGIC.required_helper_count == 0;

    ShaderPermutationKey key {};
    const hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(SKY_MINIMAL_COMPOSED_DEF, key);
    const hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(SKY_MINIMAL_COMPOSED_DEF, key);

    const bool vs_semantic_ok = ContainsKeyword(vs_code, "Output.Direction = normalize(vi.Position)")
                             && ContainsKeyword(vs_code, "return vec4(vi.Position, 1.0)");

    const bool fs_semantic_ok = ContainsKeyword(fs_code, "vec3 getSky(vec3 dir, vec3 to_light)")
                             && ContainsKeyword(fs_code, "vec3 getSun(vec3 dir, vec3 to_light)")
                             && ContainsKeyword(fs_code, "return vec4(sky_color + sun_color, 1.0)");

    printf("ValidateMaterialLogicDef: %s\n", logic_valid ? "PASS" : "FAIL");
    printf("Required resource shape: %s\n", required_resource_shape_ok ? "PASS" : "FAIL");
    printf("Required resource names: %s\n", required_resource_names_ok ? "PASS" : "FAIL");
    printf("Required helper dependencies: %s\n", required_helpers_ok ? "PASS" : "FAIL");
    printf("VS semantic assertions: %s\n", vs_semantic_ok ? "PASS" : "FAIL");
    printf("FS semantic assertions: %s\n", fs_semantic_ok ? "PASS" : "FAIL");

    const bool ok = logic_valid
                 && required_resource_shape_ok
                 && required_resource_names_ok
                 && required_helpers_ok
                 && vs_semantic_ok
                 && fs_semantic_ok;
    printf("\n=== SkyMinimal Template Conformance Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
