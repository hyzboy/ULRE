#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderComposition_Examples.h>
#include <hgl/shadergen/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_BasicLit_Logic.h"

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
    printf("=== BasicLit Template Conformance Test ===\n\n");

    const VertexShaderBusiness vs_business { BASIC_LIT_VS_BUSINESS };
    const FragmentShaderBusiness fs_business { BASIC_LIT_FS_BUSINESS };

    ComposedMaterialDef base_def = EX_BASIC_LIT_COMPOSED;
    base_def.name = "BasicLitTemplate";
    base_def.vertex_business = &vs_business;
    base_def.fragment_business = &fs_business;
    base_def.enable_lighting = false;

    const bool logic_valid = ValidateMaterialLogicDef(BASIC_LIT_LOGIC);

    const bool required_resource_shape_ok =
        BASIC_LIT_VERTEX_SHADER_LOGIC.required_resource_count == 2
        && BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resource_count == 6;

    const bool required_resource_names_ok =
        EqualsString(BASIC_LIT_VERTEX_SHADER_LOGIC.required_resources[0], "camera")
        && EqualsString(BASIC_LIT_VERTEX_SHADER_LOGIC.required_resources[1], "l2w")
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resources[0], "camera")
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resources[1], "sky")
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resources[2], "mtl")
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resources[3], "TextureBaseColor")
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resources[4], "TextureNormal")
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_resources[5], "TextureRoughness");

    const bool required_helpers_ok =
        BASIC_LIT_VERTEX_SHADER_LOGIC.required_helper_count == 0
        && BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_helper_count == 1
        && EqualsString(BASIC_LIT_FRAGMENT_SHADER_LOGIC.required_helpers[0], "GetMI");

    ShaderPermutationKey key {};
    const std::string vs_code = ComposedShaderGenerator::ComposeVertexShader(base_def, key);
    const std::string fs_code = ComposedShaderGenerator::ComposeFragmentShader(base_def, key);

    const bool vs_semantic_ok = ContainsKeyword(vs_code.c_str(), "Output.TexCoord = vi.TexCoord")
                             && ContainsKeyword(vs_code.c_str(), "GetLocalToWorld()")
                             && ContainsKeyword(vs_code.c_str(), "return vec4(vi.Position, 1.0)");

    const bool fs_semantic_ok = ContainsKeyword(fs_code.c_str(), "ResolveRuntimeNormalStrength(mi.normal_strength)")
                             && ContainsKeyword(fs_code.c_str(), "ULRE_GetSkyLightDir")
                             && ContainsKeyword(fs_code.c_str(), "ULRE_GetSkyLightColor")
                             && ContainsKeyword(fs_code.c_str(), "ULRE_GetSkyAmbientColor")
                             && ContainsKeyword(fs_code.c_str(), "return vec4(color, 1.0)");

    const bool ibl_branch_semantic_ok = !ContainsKeyword(fs_code.c_str(), "#if ULRE_SKYLIGHT_MODEL == ULRE_SKYLIGHT_MODEL_IBL")
                                     || ContainsKeyword(fs_code.c_str(), "color += mi.ibl_intensity * sky.base_sky_color.rgb");

    printf("ValidateMaterialLogicDef: %s\n", logic_valid ? "PASS" : "FAIL");
    printf("Required resource shape: %s\n", required_resource_shape_ok ? "PASS" : "FAIL");
    printf("Required resource names: %s\n", required_resource_names_ok ? "PASS" : "FAIL");
    printf("Required helper dependencies: %s\n", required_helpers_ok ? "PASS" : "FAIL");
    printf("VS semantic assertions: %s\n", vs_semantic_ok ? "PASS" : "FAIL");
    printf("FS semantic assertions: %s\n", fs_semantic_ok ? "PASS" : "FAIL");
    printf("IBL branch semantic assertions: %s\n", ibl_branch_semantic_ok ? "PASS" : "FAIL");

    const bool ok = logic_valid
                 && required_resource_shape_ok
                 && required_resource_names_ok
                 && required_helpers_ok
                 && vs_semantic_ok
                 && fs_semantic_ok
                 && ibl_branch_semantic_ok;
    printf("\n=== BasicLit Template Conformance Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
