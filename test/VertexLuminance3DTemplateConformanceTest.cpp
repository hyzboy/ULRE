#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_VertexLuminance3D.h"
#include "ShaderGen/3d/S_VertexLuminance3D_Logic.h"

#include <cstdio>
#include <cstring>

using namespace hgl::graph::mtl;

static bool ContainsKeyword(const char *text, const char *keyword)
{
    return text && keyword && (std::strstr(text, keyword) != nullptr);
}

static bool EqualsString(const char *lhs, const char *rhs)
{
    if (!lhs || !rhs)
        return false;

    return std::strcmp(lhs, rhs) == 0;
}

int main()
{
    printf("=== VertexLuminance3D Template Conformance Test ===\n\n");

    const bool logic_valid = ValidateMaterialLogicDef(VERTEX_LUMINANCE_3D_LOGIC);

    const bool required_resource_shape_ok =
        VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC.required_resource_count == 3
        && VERTEX_LUMINANCE_3D_FRAGMENT_SHADER_LOGIC.required_resource_count == 0;

    const bool required_resource_names_ok =
        EqualsString(VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC.required_resources[0], "camera")
        && EqualsString(VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC.required_resources[1], "l2w")
        && EqualsString(VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC.required_resources[2], "mtl");

    const bool required_helpers_ok =
        VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC.required_helper_count == 1
        && EqualsString(VERTEX_LUMINANCE_3D_VERTEX_SHADER_LOGIC.required_helpers[0], "GetMI")
        && VERTEX_LUMINANCE_3D_FRAGMENT_SHADER_LOGIC.required_helper_count == 0;

    ShaderPermutationKey key {};
    const std::string vs_code_vec3 = ComposedShaderGenerator::ComposeVertexShader(VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC3, key);
    const std::string vs_code_vec2 = ComposedShaderGenerator::ComposeVertexShader(VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC2, key);
    const std::string fs_code = ComposedShaderGenerator::ComposeFragmentShader(VERTEX_LUMINANCE_3D_COMPOSED_DEF_VEC2, key);

    const bool vs_semantic_ok = ContainsKeyword(vs_code_vec3.c_str(), "MaterialInstance mi = GetMI()")
                             && ContainsKeyword(vs_code_vec3.c_str(), "Output.Color = vi.Luminance * mi.Color")
                             && ContainsKeyword(vs_code_vec3.c_str(), "return vec4(vi.Position")
                             && ContainsKeyword(vs_code_vec2.c_str(), "MaterialInstance mi = GetMI()")
                             && ContainsKeyword(vs_code_vec2.c_str(), "Output.Color = vi.Luminance * mi.Color")
                             && ContainsKeyword(vs_code_vec2.c_str(), "return vec4(vi.Position");

    const bool fs_semantic_ok = ContainsKeyword(fs_code.c_str(), "return Input.Color");

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
    printf("\n=== VertexLuminance3D Template Conformance Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
