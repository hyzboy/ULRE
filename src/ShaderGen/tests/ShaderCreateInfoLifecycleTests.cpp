// ShaderCreateInfo lifecycle tests.
//
// Focus:
//  1) Recompile path should produce SPV for valid GLSL when compiler is available.
//  2) Compiling empty GLSL should clear previously cached SPV payload.

#include <hgl/shadergen/ShaderCreateInfoVertex.h>
#include <hgl/shadergen/MaterialDescriptorDB.h>

#include "../GLSLCompiler.h"

#include <cstdio>

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

using namespace hgl::graph;

namespace
{

const char *kMinimalVertexGLSL =
    "#version 450\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "}\n";

const char *kInvalidVertexGLSL =
    "#version 450\n"
    "void main()\n"
    "{\n"
    "    this_is_invalid_glsl;\n"
    "}\n";

void test_empty_glsl_clears_cached_spv()
{
    if (!InitShaderCompiler())
    {
        std::fprintf(stdout,
                     "[SKIP] ShaderCreateInfoLifecycleTests: GLSLCompiler plugin not available.\n");
        return;
    }

    MaterialDescriptorDB descriptor_db;
    ShaderCreateInfoVertex shader(&descriptor_db);

    constexpr int kPressureRounds = 16;
    for (int i = 0; i < kPressureRounds; ++i)
    {
        shader.SetFinalGLSL(kMinimalVertexGLSL);
        CHECK_TRUE(shader.CompileFinalGLSLToSPV());
        CHECK_TRUE(shader.GetSPVData() != nullptr);
        CHECK_TRUE(shader.GetSPVSize() > 0);

        shader.SetFinalGLSL(kInvalidVertexGLSL);
        CHECK_TRUE(!shader.CompileFinalGLSLToSPV());
        CHECK_TRUE(shader.GetSPVData() == nullptr);
        CHECK_TRUE(shader.GetSPVSize() == 0);

        shader.SetFinalGLSL("");
        CHECK_TRUE(!shader.CompileFinalGLSLToSPV());
        CHECK_TRUE(shader.GetSPVData() == nullptr);
        CHECK_TRUE(shader.GetSPVSize() == 0);
    }

    CloseShaderCompiler();
}

} // namespace

int main()
{
    test_empty_glsl_clears_cached_spv();

    if (g_failures == 0)
        std::printf("All tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
