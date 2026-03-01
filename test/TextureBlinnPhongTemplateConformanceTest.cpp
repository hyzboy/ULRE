#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderComposition_Examples.h>
#include <hgl/shadergen/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_TextureBlinnPhong_Logic.h"

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
    printf("=== TextureBlinnPhong Template Conformance Test ===\n\n");

    const VertexShaderBusiness vs_business { TEXTURE_BLINN_PHONG_VS_BUSINESS };
    const FragmentShaderBusiness fs_business { TEXTURE_BLINN_PHONG_FS_BUSINESS };

    ComposedMaterialDef base_def = EX_BASIC_LIT_COMPOSED;
    base_def.name = "TextureBlinnPhongTemplate";
    base_def.vertex_business = &vs_business;
    base_def.fragment_business = &fs_business;
    base_def.enable_lighting = false;

    const bool logic_valid = ValidateMaterialLogicDef(TEXTURE_BLINN_PHONG_LOGIC);

    const bool required_resource_shape_ok =
        TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC.required_resource_count == 2
        && TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resource_count == 6;

    const bool required_resource_names_ok =
        EqualsString(TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC.required_resources[0], "camera")
        && EqualsString(TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC.required_resources[1], "l2w")
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resources[0], "camera")
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resources[1], "sky")
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resources[2], "mtl")
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resources[3], "TextureBaseColor")
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resources[4], "TextureNormal")
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_resources[5], "TextureRoughness");

    const bool required_helpers_ok =
        TEXTURE_BLINN_PHONG_VERTEX_SHADER_LOGIC.required_helper_count == 0
        && TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_helper_count == 1
        && EqualsString(TEXTURE_BLINN_PHONG_FRAGMENT_SHADER_LOGIC.required_helpers[0], "GetMI");

    ShaderPermutationKey key {};
    const hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(base_def, key);
    const hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(base_def, key);

    const bool vs_semantic_ok = ContainsKeyword(vs_code, "Output.TexCoord = vi.TexCoord")
                             && ContainsKeyword(vs_code, "GetLocalToWorld()")
                             && ContainsKeyword(vs_code, "return vec4(vi.Position, 1.0)");

    const bool fs_semantic_ok = ContainsKeyword(fs_code, "ResolveSurfaceUV")
                             && ContainsKeyword(fs_code, "ResolveSurfaceNormal")
                             && ContainsKeyword(fs_code, "fresnelSchlick")
                             && ContainsKeyword(fs_code, "texture(TextureNormal")
                             && ContainsKeyword(fs_code, "return vec4(color, 1.0)");

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
    printf("\n=== TextureBlinnPhong Template Conformance Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
