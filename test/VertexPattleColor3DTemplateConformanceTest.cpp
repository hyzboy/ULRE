#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_VertexPattleColor3D.h"
#include "ShaderGen/3d/S_VertexPattleColor3D_Logic.h"

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
    printf("=== VertexPattleColor3D Template Conformance Test ===\n\n");

    const bool logic_valid = ValidateMaterialLogicDef(VERTEX_PATTLE_COLOR_3D_LOGIC);

    const bool required_resource_shape_ok =
        VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC.required_resource_count == 3
        && VERTEX_PATTLE_COLOR_3D_FRAGMENT_SHADER_LOGIC.required_resource_count == 0;

    const bool required_resource_names_ok =
        EqualsString(VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC.required_resources[0], "l2w")
        && EqualsString(VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC.required_resources[1], "camera")
        && EqualsString(VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC.required_resources[2], "color_pattle");

    const bool required_helpers_ok =
        VERTEX_PATTLE_COLOR_3D_VERTEX_SHADER_LOGIC.required_helper_count == 0
        && VERTEX_PATTLE_COLOR_3D_FRAGMENT_SHADER_LOGIC.required_helper_count == 0;

    ShaderPermutationKey key {};
    const hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(VERTEX_PATTLE_COLOR_3D_COMPOSED_DEF, key);
    const hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(VERTEX_PATTLE_COLOR_3D_COMPOSED_DEF, key);

    const bool vs_semantic_ok = ContainsKeyword(vs_code, "Output.Color = color_pattle.color[vi.Color.r]")
                             && ContainsKeyword(vs_code, "return vec4(vi.Position, 1.0)");

    const bool fs_semantic_ok = ContainsKeyword(fs_code, "return Input.Color");

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
    printf("\n=== VertexPattleColor3D Template Conformance Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
