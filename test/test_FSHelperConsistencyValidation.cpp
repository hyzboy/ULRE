#include <hgl/graph/mtl/MaterialCompiler.h>
#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderComposition_Examples.h>

#include <cstdio>
#include <cstring>

using namespace hgl::graph::mtl;

namespace {

static bool RunPositiveCase(const ShaderPermutationKey &key)
{
    static const char FS_BUSINESS_OK[] = R"(
vec4 FragmentShaderBusiness(const VS_Output vso) {
    vec3 view_dir = normalize(GetCameraPosition() - GetWorldPosition());
    float ndv = max(dot(normalize(vso.WorldNormal), view_dir), 0.0);
    return vec4(vec3(ndv), 1.0);
}
)";

    ComposedMaterialDef def = EX_BASIC_LIT_COMPOSED;
    const FragmentShaderBusiness fs_business{FS_BUSINESS_OK};
    def.fragment_business = &fs_business;

    const hgl::AnsiString generated_fs = ComposedShaderGenerator::ComposeFragmentShader(def, key);
    const bool ok = ValidateFSMainBusinessHelperConsistency(def, generated_fs);

    std::printf("[Case +] builtins required by FS business: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

static bool RunNegativeCase(const ShaderPermutationKey &key)
{
    static const char FS_BUSINESS_MISSING[] = R"(
vec4 FragmentShaderBusiness(const VS_Output vso) {
    vec3 hacked = MissingCustomHelper(vso.WorldNormal);
    return vec4(hacked, 1.0);
}
)";

    static const char *FS_REQUIRED_HELPERS[] = {
        "MissingCustomHelper",
    };

    ComposedMaterialDef def = EX_BASIC_LIT_COMPOSED;
    const FragmentShaderBusiness fs_business{FS_BUSINESS_MISSING};
    def.fragment_business = &fs_business;
    def.fragment_required_helpers = FS_REQUIRED_HELPERS;
    def.fragment_required_helper_count = 1;

    const hgl::AnsiString generated_fs = ComposedShaderGenerator::ComposeFragmentShader(def, key);
    const bool ok = ValidateFSMainBusinessHelperConsistency(def, generated_fs);

    std::printf("[Case -] missing FS helper must be blocked: %s\n", !ok ? "PASS" : "FAIL");
    return !ok;
}

} // namespace

int main()
{
    const ShaderPermutationKey key;

    const bool positive_ok = RunPositiveCase(key);
    const bool negative_ok = RunNegativeCase(key);

    const bool all_ok = positive_ok && negative_ok;
    std::printf("\n=== FS helper consistency validation: %s ===\n", all_ok ? "PASS" : "FAIL");
    return all_ok ? 0 : 1;
}
