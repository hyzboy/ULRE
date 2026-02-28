#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_Gizmo3D.h"

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
    printf("=== Gizmo3D Template Conformance Test ===\n\n");

    const bool logic_valid = ValidateMaterialLogicDef(GIZMO_3D_LOGIC);

    const bool required_resource_shape_ok =
        GIZMO_3D_VERTEX_SHADER_LOGIC.required_resource_count == 2
        && GIZMO_3D_FRAGMENT_SHADER_LOGIC.required_resource_count == 2;

    const bool required_resource_names_ok =
        EqualsString(GIZMO_3D_VERTEX_SHADER_LOGIC.required_resources[0], "l2w")
        && EqualsString(GIZMO_3D_VERTEX_SHADER_LOGIC.required_resources[1], "camera")
        && EqualsString(GIZMO_3D_FRAGMENT_SHADER_LOGIC.required_resources[0], "MaterialInstanceData")
        && EqualsString(GIZMO_3D_FRAGMENT_SHADER_LOGIC.required_resources[1], "camera");

    const bool required_helpers_ok =
        GIZMO_3D_VERTEX_SHADER_LOGIC.required_helper_count == 2
        && EqualsString(GIZMO_3D_VERTEX_SHADER_LOGIC.required_helpers[0], "GetNormal")
        && EqualsString(GIZMO_3D_VERTEX_SHADER_LOGIC.required_helpers[1], "GetLocalToWorld")
        && GIZMO_3D_FRAGMENT_SHADER_LOGIC.required_helper_count == 1
        && EqualsString(GIZMO_3D_FRAGMENT_SHADER_LOGIC.required_helpers[0], "GetMI");

    ShaderPermutationKey key {};
    const hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(GIZMO_3D_COMPOSED_DEF, key);
    const hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(GIZMO_3D_COMPOSED_DEF, key);

    const bool vs_semantic_ok = ContainsKeyword(vs_code, "Output.Normal = GetNormal(vi.Normal)")
                             && ContainsKeyword(vs_code, "Output.Position = GetLocalToWorld() * vec4(vi.Position, 1.0)")
                             && ContainsKeyword(vs_code, "return vec4(vi.Position, 1.0)");

    const bool fs_semantic_ok = ContainsKeyword(fs_code, "MaterialInstance mi = GetMI()")
                             && ContainsKeyword(fs_code, "SUN_DIRECTION")
                             && ContainsKeyword(fs_code, "return vec4(direct_color + spec_color, 1.0)");

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
    printf("\n=== Gizmo3D Template Conformance Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
