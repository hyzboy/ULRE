// Shader compiler multi-thread smoke tests.
//
// Focus:
//  1) Repeated parallel CompileShader calls should not crash/hang.
//  2) Successful compiles should return non-empty SPV payloads.

#include <hgl/common/ShaderStageDef.h>

#include "../GLSLCompiler.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

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

void test_parallel_compile_smoke()
{
    if (!InitShaderCompiler())
    {
        std::fprintf(stdout,
                     "[SKIP] ShaderCompilerThreadSmokeTests: GLSLCompiler plugin not available.\n");
        return;
    }

    constexpr int kThreadCount = 4;
    constexpr int kRoundsPerThread = 8;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    auto worker = [&]()
    {
        for (int i = 0; i < kRoundsPerThread; ++i)
        {
            SPVData *spv = CompileShader(uint32_t(ShaderStage::Vertex), kMinimalVertexGLSL);

            if (!spv)
            {
                ++failure_count;
                continue;
            }

            if (spv->spv_data == nullptr || spv->spv_length == 0)
            {
                ++failure_count;
                FreeSPVData(spv);
                continue;
            }

            ++success_count;
            FreeSPVData(spv);
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(kThreadCount);

    for (int i = 0; i < kThreadCount; ++i)
        workers.emplace_back(worker);

    for (auto &t : workers)
        t.join();

    CHECK_TRUE(success_count.load() > 0);
    CHECK_TRUE(failure_count.load() == 0);

    CloseShaderCompiler();
}

} // namespace

int main()
{
    test_parallel_compile_smoke();

    if (g_failures == 0)
        std::printf("All tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
