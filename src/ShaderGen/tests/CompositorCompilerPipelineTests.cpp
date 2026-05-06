// CompositorCompiler pipeline tests.
//
// Focus:
//  1) Descriptor contract invariants are populated during snapshot preparation.
//  2) Layout define injection is present in prepared GLSL (snapshot + reflection path).

#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/internal/CompositorMaterialPreparation.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>
#include <hgl/mtl/StaticMaterialDef.h>

#include <cstdio>
#include <string>
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
    "    outColor = vec4(1.0);\n"
    "}\n";

bool Contains(const std::string &text, const std::string &needle)
{
    return text.find(needle) != std::string::npos;
}

StaticMaterialDef BuildPipelineTestDef()
{
    static UBOSemanticSet ubos = {
        UBODescriptorSemantic::CameraInfo,
    };

    static SSBOSemanticSet ssbos = {
        SSBODescriptorSemantic::TransformData,
    };

    static StaticTextureSamplerDescriptors samplers = {
        {
            SamplerSlot::BaseColor,
            MakeStaticTextureSamplerDescriptor(SamplerType::Sampler2D,
                                               0,
                                               0,
                                               TextureChannelHint::RGBA),
        },
    };

    StaticMaterialDef def{};
    def.name = "CompositorCompilerPipelineTest";
    def.primitive_type = PrimitiveType::Triangles;
    def.vertex_entries = nullptr;
    def.vertex_entry_count = 0;
    def.ubo_descriptors = &ubos;
    def.ssbo_descriptors = &ssbos;
    def.texture_samplers = &samplers;
    def.shader_data_schema = ShaderDataSchema::None;
    def.vertex_stream_key = nullptr;
    return def;
}

void test_prepare_snapshot_builds_binding_contract_and_layout_defines()
{
    const StaticMaterialDef def = BuildPipelineTestDef();

    std::string diagnostics;
    std::unique_ptr<MaterialCreateInfo> mci = internal::PrepareCompositorMaterialSnapshotOwned(
        nullptr,
        def,
        kVertexGLSL,
        kFragmentGLSL,
        nullptr,
        &diagnostics);

    CHECK_TRUE(mci != nullptr);
    if (!mci)
        return;

    CHECK_TRUE(diagnostics.empty());

    const DescriptorBindingSlots &contract = mci->GetBindingContract();

    std::vector<std::string> contract_diagnostics;
    CHECK_TRUE(ValidateBindingContract(contract, contract_diagnostics));
    CHECK_TRUE(contract_diagnostics.empty());

    const uint32_t expected_stage_bits =
        uint32_t(ShaderStage::Vertex) | uint32_t(ShaderStage::Fragment);

    CHECK_EQ(contract.ubos[size_t(UBODescriptorSemantic::CameraInfo)], expected_stage_bits);
    CHECK_EQ(contract.ssbos[size_t(SSBODescriptorSemantic::TransformData)], expected_stage_bits);

    const ShaderCreateInfoVertex *vert = mci->GetVertexShader();
    const ShaderCreateInfo *frag = mci->GetStageShader(ShaderStage::Fragment);

    CHECK_TRUE(vert != nullptr);
    CHECK_TRUE(frag != nullptr);
    if (!vert || !frag)
        return;

    CHECK_TRUE(Contains(vert->GetFinalGLSL(), "#define CAMERA_BINDING "));
    CHECK_TRUE(Contains(vert->GetFinalGLSL(), "#define TRANSFORM_DATA_BINDING "));
    CHECK_TRUE(Contains(frag->GetFinalGLSL(), "#define CAMERA_BINDING "));
    CHECK_TRUE(Contains(frag->GetFinalGLSL(), "#define TRANSFORM_DATA_BINDING "));
}

void test_reflection_prepare_path_retains_layout_define_injection()
{
    const StaticMaterialDef def = BuildPipelineTestDef();

    std::string out_vs;
    std::string out_fs;
    std::string diagnostics;

    const bool ok = PrepareCompositorGLSLForReflection(def,
                                                       kVertexGLSL,
                                                       kFragmentGLSL,
                                                       out_vs,
                                                       out_fs,
                                                       &diagnostics);

    CHECK_TRUE(ok);
    if (!ok)
        return;

    CHECK_TRUE(diagnostics.empty());

    CHECK_TRUE(Contains(out_vs, "#define CAMERA_BINDING "));
    CHECK_TRUE(Contains(out_vs, "#define TRANSFORM_DATA_BINDING "));
    CHECK_TRUE(Contains(out_fs, "#define CAMERA_BINDING "));
    CHECK_TRUE(Contains(out_fs, "#define TRANSFORM_DATA_BINDING "));

    const size_t vs_version_pos = out_vs.find("#version");
    const size_t vs_define_pos = out_vs.find("#define CAMERA_BINDING ");
    CHECK_TRUE(vs_version_pos != std::string::npos);
    CHECK_TRUE(vs_define_pos != std::string::npos);
    if (vs_version_pos != std::string::npos && vs_define_pos != std::string::npos)
        CHECK_TRUE(vs_version_pos < vs_define_pos);
}

} // namespace

int main()
{
    test_prepare_snapshot_builds_binding_contract_and_layout_defines();
    test_reflection_prepare_path_retains_layout_define_injection();

    if (g_failures == 0)
        std::printf("All tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) FAILED.\n", g_failures);

    return g_failures;
}
