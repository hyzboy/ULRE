#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>

#include "GLSLCompiler.h"

#include <cstdio>
#include <type_traits>

using namespace hgl::graph;
using namespace hgl::graph::mtl;
using namespace hgl::graph::mtl::contract;

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))

template<typename TResult>
static void PrintBuildResult(const char *name,const TResult &result)
{
    if constexpr (std::is_pointer_v<decltype(result.value)>)
    {
        std::fprintf(stdout,
                     "[Smoke] %s: success=%d value=%p diagnostics=%zu\n",
                     name,
                     result.success?1:0,
                     static_cast<const void *>(result.value),
                     result.diagnostics.size());
    }
    else
    {
        std::fprintf(stdout,
                     "[Smoke] %s: success=%d final_state=%u layout_finalized=%d descriptor_count=%u binaries=%zu diagnostics=%zu\n",
                     name,
                     result.success?1:0,
                     (unsigned)result.value.final_state,
                     result.value.layout_finalized?1:0,
                     result.value.descriptor_count,
                     result.value.binaries.size(),
                     result.diagnostics.size());
    }

    for(const auto &d:result.diagnostics)
    {
        std::fprintf(stdout,
                     "[Smoke][Diag] code=%u stage=0x%08X subject=%s message=%s\n",
                     (unsigned)d.code,
                     (unsigned)d.stage,
                     d.subject.c_str(),
                     d.message.c_str());
    }
}

static MaterialCreateConfig MakeBasicConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Vertex);
    return cfg;
}

static MaterialCreateConfig MakeFragmentConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Fragment);
    return cfg;
}

static MaterialCreateConfig MakeUnsupportedStageConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Compute);
    return cfg;
}

static MaterialCreateConfig MakeMaterialInstanceConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    cfg.material_instance = true;
    return cfg;
}

static MaterialCreateConfig MakeLocalToWorldConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,true);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    cfg.local_to_world = true;
    return cfg;
}

static PhysicalDeviceProfileLite MakeBasicProfile()
{
    PhysicalDeviceProfileLite profile{};
    profile.name = "SmokeProfile";
    profile.target_vulkan_version = 0;
    profile.target_spv_version = 0;
    return profile;
}

static ShaderBuildDescriptorSpec MakeMaterialInstanceDescriptorSpec()
{
    ShaderBuildDescriptorSpec spec{};
    spec.material_instance_bytes = 64;
    return spec;
}

static ShaderBuildDescriptorSpec MakeMaterialInstanceSchemaDescriptorSpec()
{
    ShaderBuildDescriptorSpec spec{};
    spec.material_instance_schema = ShaderDataSchema::Color4f;
    spec.material_instance_bytes = GetShaderDataSchemaInfo(spec.material_instance_schema).byte_size;
    return spec;
}

static StaticMaterialDef MakeSchemaAwareCompositorDef()
{
    static FixedVertexEntry vertex_entries[] =
    {
        { VAT_VEC3, VertexAttrib::Position }
    };

    static UBOSemanticSet ubos =
    {
        UBODescriptorSemantic::ViewportInfo,
        UBODescriptorSemantic::CameraInfo
    };

    static SSBOSemanticSet ssbos =
    {
        SSBODescriptorSemantic::TransformData
    };

    static StaticTextureSamplerDescriptors samplers =
    {
        { SamplerSlot::BaseColor, MakeStaticTextureSamplerDescriptor(SamplerType::Sampler2D) }
    };

    StaticMaterialDef def{};
    def.name = "SchemaAwareSmokeCompositor";
    def.primitive_type = PrimitiveType::Triangles;
    def.vertex_entries = vertex_entries;
    def.vertex_entry_count = 1;
    def.ubo_descriptors = &ubos;
    def.ssbo_descriptors = &ssbos;
    def.texture_samplers = &samplers;
    def.shader_data_schema = ShaderDataSchema::Color4f;
    return def;
}

static StaticMaterialDef MakeMinimalProductDef()
{
    static FixedVertexEntry vertex_entries[] =
    {
        { VAT_VEC3, VertexAttrib::Position }
    };

    StaticMaterialDef def{};
    def.name = "MinimalPipelineProduct";
    def.primitive_type = PrimitiveType::Triangles;
    def.vertex_entries = vertex_entries;
    def.vertex_entry_count = 1;
    return def;
}

static void TestBuildFailsWhenStageBitsIsZero()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = 0;

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("StageBitsIsZero",result);

    CHECK_TRUE(!result.success);
    CHECK_TRUE(!result.diagnostics.empty());
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
}

static void TestBuildFailsWhenProfileNull()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeBasicConfig();

    auto result = pipeline.Build(cfg,nullptr);
    PrintBuildResult("ProfileNull",result);

    CHECK_TRUE(!result.success);
    CHECK_TRUE(!result.diagnostics.empty());
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
}

static void TestBuildMinimalVertexPath()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeBasicConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("MinimalVertexPath",result);

    CHECK_TRUE(result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Compiled);
    CHECK_TRUE(result.value.layout_finalized);
    CHECK_TRUE(!result.value.binaries.empty());
    CHECK_TRUE(!result.value.binaries[0].spirv.empty());
}

static void TestBuildMinimalFragmentPath()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeFragmentConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("MinimalFragmentPath",result);

    CHECK_TRUE(result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Compiled);
    CHECK_TRUE(result.value.layout_finalized);
    CHECK_TRUE(!result.value.binaries.empty());
    CHECK_TRUE(!result.value.binaries[0].spirv.empty());
}

static void TestBuildFailsWhenStageUnsupportedByMinimalPipeline()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeUnsupportedStageConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("StageUnsupportedByMinimalPipeline",result);

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
}

static void TestBuildProductForMinimalPipelineMaterial()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeBasicConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeMinimalProductDef();

    auto result = pipeline.BuildProduct(
        def,
        cfg,
        &profile,
        "#version 450\nvoid main(){}\n",
        "#version 450\nlayout(location=0) out vec4 outColor; void main(){outColor=vec4(1.0);}\n");

    PrintBuildResult("MinimalPipelineProduct",result);

    CHECK_TRUE(result.success);
    CHECK_TRUE(result.value != nullptr);

    if(!result.success || !result.value)
        return;

    MaterialCreateInfo *mci = result.value;
    CHECK_TRUE(mci->GetStageShader(ShaderStage::Vertex) != nullptr);
    CHECK_TRUE(mci->GetStageShader(ShaderStage::Fragment) != nullptr);
    CHECK_TRUE(mci->GetVertexShader() != nullptr);
    CHECK_TRUE(!mci->GetShaderMap().IsEmpty());
    CHECK_TRUE(!mci->GetVertexShader()->GetFinalGLSL().empty());
    CHECK_TRUE(!mci->GetStageShader(ShaderStage::Fragment)->GetFinalGLSL().empty());
    CHECK_TRUE(mci->GetStageShader(ShaderStage::Vertex)->GetSPVSize() > 0);
    CHECK_TRUE(mci->GetStageShader(ShaderStage::Fragment)->GetSPVSize() > 0);

    delete mci;
}

static void TestBuildMaterialCreateInfoForSchemaAwareCompositor()
{
    ShaderBuildPipeline pipeline;
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();

    auto result = pipeline.BuildProduct(
        def,
        cfg,
        &profile,
        "#version 450\nvoid main(){}\n",
        "#version 450\nlayout(location=0) out vec4 outColor; void main(){outColor=vec4(1.0);}\n");

    CHECK_TRUE(result.success);
    CHECK_TRUE(result.value != nullptr);

    if(!result.success || !result.value)
        return;

    MaterialCreateInfo *mci = result.value;
    CHECK_TRUE(mci->GetVertexShader() != nullptr);
    CHECK_TRUE(mci->GetStageShader(ShaderStage::Fragment) != nullptr);
    CHECK_TRUE(mci->GetDescriptorInfo().GetCount() > 0);
    CHECK_TRUE(mci->HasLocalToWorld());
    CHECK_EQ((int)mci->GetMaterialInstance().schema, (int)ShaderDataSchema::Color4f);
    CHECK_TRUE(!mci->GetMaterialInstance().schema_file.empty());
    CHECK_TRUE(!mci->GetShaderMap().IsEmpty());

    delete mci;
}

static void TestBuildProductParityForMaterialInstanceConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.SetMaterialInstance(spec.material_instance_bytes,cfg.shader_stage_flag_bit));

    MaterialCreateInfo *legacy_mci = builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci != nullptr);
    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());
    CHECK_EQ(pipeline_result.value.material_instance.stride, legacy_mci->GetMaterialInstance().stride);
    CHECK_EQ(pipeline_result.value.material_instance.stage_bits, legacy_mci->GetMaterialInstance().stage_bits);

    delete legacy_mci;
}

int main()
{
    if(!hgl::graph::InitShaderCompiler())
    {
        std::fprintf(stderr,"Failed to initialize shader compiler.\n");
        return 1;
    }

    TestBuildFailsWhenStageBitsIsZero();
    TestBuildFailsWhenProfileNull();
    TestBuildMinimalVertexPath();
    TestBuildMinimalFragmentPath();
    TestBuildFailsWhenStageUnsupportedByMinimalPipeline();
    TestBuildProductForMinimalPipelineMaterial();
    TestBuildMaterialCreateInfoForSchemaAwareCompositor();
    TestBuildProductParityForMaterialInstanceConfig();

    hgl::graph::CloseShaderCompiler();
    return g_failures;
}
