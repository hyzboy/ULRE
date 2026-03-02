#include <hgl/graph/module/ShaderGenPathMode.h>
#include <cstdio>
#include <cstring>

using namespace hgl::graph;

int main()
{
    int failed = 0;

    auto CheckMode = [&failed](const char *input, const ShaderGenPathMode expected, const char *name_expected)
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
    };

    CheckMode(nullptr,                ShaderGenPathMode::MirrorValidate, "mirror-validate");
    CheckMode("",                    ShaderGenPathMode::MirrorValidate, "mirror-validate");
    CheckMode("legacy-only",         ShaderGenPathMode::LegacyOnly,     "legacy-only");
    CheckMode("mirror-validate",     ShaderGenPathMode::MirrorValidate, "mirror-validate");
    CheckMode("mirror-preferred",    ShaderGenPathMode::MirrorPreferred,"mirror-preferred");
    CheckMode("unknown-value",       ShaderGenPathMode::MirrorValidate, "mirror-validate");

    if (failed != 0)
    {
        std::fprintf(stderr, "ShaderGenPathModeTest FAILED (%d)\n", failed);
        return 1;
    }

    std::fprintf(stdout, "ShaderGenPathModeTest PASSED\n");
    return 0;
}
