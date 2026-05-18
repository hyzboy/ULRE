#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/Material2DCreateConfig.h>
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
            std::fprintf(stderr, "FAIL (%s:%d): %s\n", __FILE__, __LINE__, #expr); \
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

static bool HasAnyBindingContract(const DescriptorBindingSlots &contract)
{
    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        if(contract.ubos[i]!=0)
            return true;

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        if(contract.ssbos[i]!=0)
            return true;

    return false;
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

static void TestBuildDescriptorSpecFromStaticMaterialDef()
{
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    const auto spec = ShaderBuildPipeline::BuildDescriptorSpecFromStaticMaterialDef(def);

    CHECK_EQ(spec.ubos.size(), size_t(2));
    CHECK_EQ(spec.ubos[0], UBODescriptorSemantic::ViewportInfo);
    CHECK_EQ(spec.ubos[1], UBODescriptorSemantic::CameraInfo);
    CHECK_EQ(spec.ssbos.size(), size_t(0));
    CHECK_EQ(spec.material_instance_schema, ShaderDataSchema::Color4f);
    CHECK_EQ(spec.material_instance_bytes, GetShaderDataSchemaInfo(ShaderDataSchema::Color4f).byte_size);
}

static void TestBuildConfigFromStaticMaterialDefMergesStaticNeeds()
{
    static StaticTextureSamplerDescriptors samplers =
    {
        { SamplerSlot::BaseColor, MakeStaticTextureSamplerDescriptor(SamplerType::Sampler2D) }
    };

    static SSBOSemanticSet ssbos =
    {
        SSBODescriptorSemantic::TransformData
    };

    StaticMaterialDef def{};
    def.name = "ConfigMergeSmoke";
    def.primitive_type = PrimitiveType::Triangles;
    def.texture_samplers = &samplers;
    def.ssbo_descriptors = &ssbos;

    MaterialCreateConfig cfg(PrimitiveType::Points,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Vertex);

    const auto merged = ShaderBuildPipeline::BuildConfigFromStaticMaterialDef(def,&cfg);

    CHECK_EQ(merged.prim, PrimitiveType::Triangles);
    CHECK_EQ(merged.shader_stage_flag_bit, uint32_t(ShaderStage::Vertex));
    CHECK_TRUE(merged.material_instance);
    CHECK_TRUE(merged.local_to_world);
    CHECK_TRUE(merged.HasTextureSourceBitsOverride() || merged.sampler_feature_bits_override != 0);
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
    CHECK_TRUE(mci->GetVertexShader()->GetInput().count > 0);

    delete mci;
}

static void TestBuildMaterialCreateInfoForSchemaAwareCompositor()
{
    ShaderBuildPipeline pipeline;
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeL2W::With);
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

    PrintBuildResult("SchemaAwareCompositorProduct",result);

    CHECK_TRUE(result.success);
    CHECK_TRUE(result.value != nullptr);
    if(!result.success || !result.value)
        return;

    MaterialCreateInfo *mci = result.value;
    CHECK_TRUE(mci->GetVertexShader() != nullptr);
    CHECK_TRUE(mci->GetStageShader(ShaderStage::Fragment) != nullptr);
    CHECK_TRUE(mci->GetDescriptorInfo().GetCount() > 0);
    CHECK_TRUE(HasAnyBindingContract(mci->GetBindingContract()));
    CHECK_TRUE(mci->HasLocalToWorld());
    CHECK_TRUE(!mci->GetLocalToWorld().enabled || mci->GetLocalToWorld().stage_bits != 0);
    CHECK_EQ((int)mci->GetMaterialInstance().schema, (int)ShaderDataSchema::Color4f);
    CHECK_TRUE(!mci->GetMaterialInstance().schema_file.empty());
    CHECK_TRUE(mci->GetMaterialInstance().stride > 0);
    CHECK_TRUE(mci->GetMaterialInstance().stage_bits != 0);
    CHECK_TRUE(!mci->GetShaderMap().IsEmpty());
    CHECK_TRUE(mci->GetVertexShader()->GetInput().count > 0);

    delete mci;
}

static void TestBuildProductConsistencyForMaterialInstanceConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    // Use the schema-aware spec so stride/schema match SchemaAwareCompositorDef.
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    auto product_result = pipeline.BuildProduct(
        def,
        cfg,
        &profile,
        "#version 450\nvoid main(){}\n",
        "#version 450\nlayout(location=0) out vec4 outColor; void main(){outColor=vec4(1.0);}\n");

    CHECK_TRUE(product_result.success);
    CHECK_TRUE(product_result.value != nullptr);
    if(!product_result.success || !product_result.value)
        return;

    CHECK_TRUE(HasAnyBindingContract(pipeline_result.value.binding_contract));
    CHECK_TRUE(HasAnyBindingContract(product_result.value->GetBindingContract()));
    // stride must match because both paths share the same ShaderDataSchema.
    CHECK_EQ(pipeline_result.value.material_instance.stride, product_result.value->GetMaterialInstance().stride);
    // stage_bits and descriptor_count intentionally not compared:
    // Build() receives only the spec (no sampler/UBO/SSBO from the def),
    // while BuildProduct() sees the full StaticMaterialDef → counts differ by design.

    delete product_result.value;
}

// ── G4 reflection tests (Step C8) ────────────────────────────────────────

// G4 happy-path: FS references Sampler_BaseColor which IS declared via
// MakeSchemaAwareCompositorDef → G4 must pass (BuildMaterialCreateInfo succeeds).
static void TestG4PassesForDeclaredSampler()
{
    ShaderBuildPipeline pipeline;
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    const MaterialCreateConfig cfg = MakeLocalToWorldConfig();
    const PhysicalDeviceProfileLite profile = MakeBasicProfile();

    // Minimal VS — no sampler use needed in vertex stage.
    const std::string vs =
        "#version 450\n"
        "layout(location=0) in vec3 inPos;\n"
        "void main(){ gl_Position = vec4(inPos,1.0); }\n";

    // FS uses Sampler_BaseColor — the declaration is injected by the pipeline;
    // do NOT redeclare it here or the GLSL compiler will report redefinition.
    const std::string fs =
        "#version 450\n"
        "layout(location=0) out vec4 outColor;\n"
        "void main(){ outColor = texture(Sampler_BaseColor, vec2(0.0)); }\n";

    auto result = pipeline.BuildMaterialCreateInfo(def, cfg, &profile, vs, fs);
    PrintBuildResult("G4PassesForDeclaredSampler", result);

    CHECK_TRUE(result.success);
    CHECK_TRUE(result.value != nullptr);
    delete result.value;
}

// G4 fatal-path: FS references Sampler_UnknownXYZ which is NOT declared →
// G4 must produce a ReflectionMismatch error and BuildMaterialCreateInfo must fail.
static void TestG4FailsForUndeclaredSampler()
{
    ShaderBuildPipeline pipeline;
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    const MaterialCreateConfig cfg = MakeLocalToWorldConfig();
    const PhysicalDeviceProfileLite profile = MakeBasicProfile();

    const std::string vs =
        "#version 450\n"
        "layout(location=0) in vec3 inPos;\n"
        "void main(){ gl_Position = vec4(inPos,1.0); }\n";

    // FS references a sampler that was never declared in the descriptor DB.
    const std::string fs =
        "#version 450\n"
        "layout(set=0,binding=99) uniform sampler2D Sampler_UnknownXYZ;\n"
        "layout(location=0) out vec4 outColor;\n"
        "void main(){ outColor = texture(Sampler_UnknownXYZ, vec2(0.0)); }\n";

    auto result = pipeline.BuildMaterialCreateInfo(def, cfg, &profile, vs, fs);
    PrintBuildResult("G4FailsForUndeclaredSampler", result);

    CHECK_TRUE(!result.success);

    bool has_reflection_mismatch = false;
    for (const auto &d : result.diagnostics)
    {
        if (d.code == ShaderGenErrorCode::ReflectionMismatch)
        {
            has_reflection_mismatch = true;
            break;
        }
    }
    CHECK_TRUE(has_reflection_mismatch);
    // result.value is nullptr on failure — no delete needed.
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
    TestBuildDescriptorSpecFromStaticMaterialDef();
    TestBuildConfigFromStaticMaterialDefMergesStaticNeeds();
    TestBuildMinimalVertexPath();
    TestBuildMinimalFragmentPath();
    TestBuildFailsWhenStageUnsupportedByMinimalPipeline();
    TestBuildProductForMinimalPipelineMaterial();
    TestBuildMaterialCreateInfoForSchemaAwareCompositor();
    TestBuildProductConsistencyForMaterialInstanceConfig();

    // ── G4 reflection validation tests (Step C8) ────────────────────────
    TestG4PassesForDeclaredSampler();
    TestG4FailsForUndeclaredSampler();

    hgl::graph::CloseShaderCompiler();
    return g_failures;
}
