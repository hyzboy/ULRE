// Material lifecycle stress tests.
//
// Focus:
//  1) Repeated build/destroy of MaterialCreateInfo snapshots should remain stable.
//  2) When GLSLCompiler plugin is available, repeated SPV compile/recompile paths
//     should preserve SPV ownership/lifecycle invariants.

#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>

#include "../GLSLCompiler.h"

#include <cstdio>
#include <memory>
#include <string>

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b) CHECK_TRUE((a) == (b))

using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{

const char *kVertexGLSL =
    "#version 450\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "}\n";

const char *kFragmentGLSL =
    "#version 450\n"
    "layout(location=0) out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "    outColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "}\n";

std::unique_ptr<MaterialCreateInfo> BuildStressSnapshotOwned()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Vertex)
                              | uint32_t(ShaderStage::Fragment);

    MaterialBuilder builder(&cfg);

    if (!builder.AddUBOStruct(cfg.shader_stage_flag_bit, UBODescriptorSemantic::CameraInfo))
        return nullptr;

    if (!builder.AddSSBOStruct(cfg.shader_stage_flag_bit, SSBODescriptorSemantic::TransformData))
        return nullptr;

    if (!builder.AddTextureSampler(cfg.shader_stage_flag_bit,
                                   SamplerType::Sampler2D,
                                   SamplerSlot::BaseColor,
                                   TextureChannelHint::RGBA))
    {
        return nullptr;
    }

    ShaderCreateInfoVertex *vert = builder.GetVertexShader();
    ShaderCreateInfo *frag = builder.GetStageShader(ShaderStage::Fragment);

    if (!vert || !frag)
        return nullptr;

    vert->SetFinalGLSL(kVertexGLSL);
    frag->SetFinalGLSL(kFragmentGLSL);

    return builder.BuildSnapshotOwned();
}

std::unique_ptr<MaterialCreateInfo> BuildStressOwnedWithSPV()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Vertex)
                              | uint32_t(ShaderStage::Fragment);

    MaterialBuilder builder(&cfg);

    if (!builder.AddUBOStruct(cfg.shader_stage_flag_bit, UBODescriptorSemantic::CameraInfo))
        return nullptr;

    if (!builder.AddSSBOStruct(cfg.shader_stage_flag_bit, SSBODescriptorSemantic::TransformData))
        return nullptr;

    if (!builder.AddTextureSampler(cfg.shader_stage_flag_bit,
                                   SamplerType::Sampler2D,
                                   SamplerSlot::BaseColor,
                                   TextureChannelHint::RGBA))
    {
        return nullptr;
    }

    ShaderCreateInfoVertex *vert = builder.GetVertexShader();
    ShaderCreateInfo *frag = builder.GetStageShader(ShaderStage::Fragment);

    if (!vert || !frag)
        return nullptr;

    vert->SetFinalGLSL(kVertexGLSL);
    frag->SetFinalGLSL(kFragmentGLSL);

    return builder.BuildOwned();
}

void test_snapshot_build_destroy_pressure()
{
    constexpr uint32_t expected_stage_bits =
        uint32_t(ShaderStage::Vertex) | uint32_t(ShaderStage::Fragment);

    constexpr int kPressureRounds = 128;

    for (int i = 0; i < kPressureRounds; ++i)
    {
        std::unique_ptr<MaterialCreateInfo> mci = BuildStressSnapshotOwned();

        CHECK_TRUE(mci != nullptr);
        if (!mci)
            continue;

        const DescriptorBindingSlots &contract = mci->GetBindingContract();
        CHECK_EQ(contract.ubos[size_t(UBODescriptorSemantic::CameraInfo)], expected_stage_bits);
        CHECK_EQ(contract.ssbos[size_t(SSBODescriptorSemantic::TransformData)], expected_stage_bits);

        ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);
        CHECK_TRUE(frag != nullptr);
        if (frag)
            CHECK_TRUE(!frag->GetFinalGLSL().empty());

        const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
        CHECK_TRUE(vert != nullptr);
        if (vert)
            CHECK_TRUE(!vert->GetFinalGLSL().empty());
    }
}

void test_compile_spv_pressure_if_compiler_available()
{
    if (!InitShaderCompiler())
    {
        std::fprintf(stdout,
                     "[SKIP] MaterialLifecycleStressTests: GLSLCompiler plugin not available.\n");
        return;
    }

    constexpr int kCompileRounds = 32;

    for (int i = 0; i < kCompileRounds; ++i)
    {
        std::unique_ptr<MaterialCreateInfo> mci = BuildStressOwnedWithSPV();

        CHECK_TRUE(mci != nullptr);
        if (!mci)
            continue;

        const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
        const ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

        CHECK_TRUE(vert != nullptr);
        CHECK_TRUE(frag != nullptr);

        if (vert)
        {
            CHECK_TRUE(vert->GetSPVData() != nullptr);
            CHECK_TRUE(vert->GetSPVSize() > 0);
        }

        if (frag)
        {
            CHECK_TRUE(frag->GetSPVData() != nullptr);
            CHECK_TRUE(frag->GetSPVSize() > 0);
        }

        ShaderCreateInfo *frag_mut = mci->GetStageShader(ShaderStage::Fragment);
        CHECK_TRUE(frag_mut != nullptr);
        if (frag_mut)
        {
            frag_mut->SetFinalGLSL("");
            CHECK_TRUE(!frag_mut->CompileFinalGLSLToSPV());
            CHECK_TRUE(frag_mut->GetSPVData() == nullptr);
            CHECK_TRUE(frag_mut->GetSPVSize() == 0);
        }
    }

    CloseShaderCompiler();
}

} // namespace

int main()
{
    test_snapshot_build_destroy_pressure();
    test_compile_spv_pressure_if_compiler_available();

    if (g_failures == 0)
        std::printf("All tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
