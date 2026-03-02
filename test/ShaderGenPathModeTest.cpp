#include <hgl/graph/module/ShaderGenPathMode.h>
#include <cstdio>
#include <cstring>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    auto CheckMode = [&failed](const char *input,
                               const ShaderGenPathMode expected,
                               const char *name_expected,
                               const bool expect_enable_validation,
                               const bool expect_require_valid,
                               const bool expect_full_diff)
    {
        const ShaderGenPathMode mode = ParseShaderGenPathMode(input);
        if (mode != expected)
        {
            std::fprintf(stderr,
                "[FAIL] ParseShaderGenPathMode('%s') unexpected mode\n",
                input ? input : "(null)");
            ++failed;
            return;
        }

        const char *name = GetShaderGenPathModeName(mode);
        if (!name || std::strcmp(name, name_expected) != 0)
        {
            std::fprintf(stderr,
                "[FAIL] GetShaderGenPathModeName mismatch for input '%s' (got='%s', expected='%s')\n",
                input ? input : "(null)",
                name ? name : "(null)",
                name_expected);
            ++failed;
        }

        const bool is_full_diff = IsShaderGenFullDiffLogEnabled(mode);
        if (is_full_diff != expect_full_diff)
        {
            std::fprintf(stderr,
                "[FAIL] IsShaderGenFullDiffLogEnabled mismatch for input '%s' (got=%d, expected=%d)\n",
                input ? input : "(null)",
                is_full_diff ? 1 : 0,
                expect_full_diff ? 1 : 0);
            ++failed;
        }

        const ShaderGenPathPolicy policy = MakeShaderGenPathPolicy(mode);
        if (policy.enable_mirror_validation != expect_enable_validation
         || policy.require_mirror_valid != expect_require_valid
         || policy.full_diff_log != expect_full_diff)
        {
            std::fprintf(stderr,
                "[FAIL] MakeShaderGenPathPolicy mismatch for input '%s' (got ev=%d rv=%d fd=%d, expected ev=%d rv=%d fd=%d)\n",
                input ? input : "(null)",
                policy.enable_mirror_validation ? 1 : 0,
                policy.require_mirror_valid ? 1 : 0,
                policy.full_diff_log ? 1 : 0,
                expect_enable_validation ? 1 : 0,
                expect_require_valid ? 1 : 0,
                expect_full_diff ? 1 : 0);
            ++failed;
        }
    };

    CheckMode(nullptr,                ShaderGenPathMode::MirrorValidate, "mirror-validate", true,  false, false);
    CheckMode("",                    ShaderGenPathMode::MirrorValidate, "mirror-validate", true,  false, false);
    CheckMode("legacy-only",         ShaderGenPathMode::LegacyOnly,     "legacy-only",     false, false, false);
    CheckMode("mirror-validate",     ShaderGenPathMode::MirrorValidate, "mirror-validate", true,  false, false);
    CheckMode("mirror-preferred",    ShaderGenPathMode::MirrorPreferred,"mirror-preferred",true,  true,  true);
    CheckMode("unknown-value",       ShaderGenPathMode::MirrorValidate, "mirror-validate", true,  false, false);

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenPathModeTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenPathModeTest PASSED\n");
    return 0;
}
