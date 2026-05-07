#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/ShaderBuildRouteSwitch.h>
#include <hgl/shadergen/CompositorCompiler.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/shadergen/MaterialBuilder.h>
#include <hgl/mtl/Material3DCreateConfig.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>

#include "GLSLCompiler.h"

#include <cstdio>
#include <fstream>

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
    std::fprintf(stdout,
                 "[Smoke] %s: success=%d final_state=%u layout_finalized=%d descriptor_count=%u binaries=%zu diagnostics=%zu\n",
                 name,
                 result.success?1:0,
                 (unsigned)result.value.final_state,
                 result.value.layout_finalized?1:0,
                 result.value.descriptor_count,
                 result.value.binaries.size(),
                 result.diagnostics.size());

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

using namespace hgl::graph;
using namespace hgl::graph::mtl;
using namespace hgl::graph::mtl::contract;

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

static MaterialCreateConfig MakeTextureSamplerOverrideConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Fragment);
    cfg.SetTextureSourceSlotEnabledOverride(SamplerSlot::BaseColor,true);
    return cfg;
}

static MaterialCreateConfig MakeTextureSamplerMultiSlotOverrideConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::Fragment);
    cfg.SetTextureSourceSlotEnabledOverride(SamplerSlot::BaseColor,true);
    cfg.SetTextureSourceSlotEnabledOverride(SamplerSlot::Normal,true);
    return cfg;
}

static MaterialCreateConfig MakeVertexFragmentTextureSamplerOverrideConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    cfg.SetTextureSourceSlotEnabledOverride(SamplerSlot::BaseColor,true);
    return cfg;
}

static MaterialCreateConfig MakeVertexFragmentLocalToWorldTextureSamplerOverrideConfig()
{
    MaterialCreateConfig cfg(PrimitiveType::Triangles,true);
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);
    cfg.local_to_world = true;
    cfg.SetTextureSourceSlotEnabledOverride(SamplerSlot::BaseColor,true);
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

static ShaderBuildDescriptorSpec MakeViewportCameraDescriptorSpec()
{
    ShaderBuildDescriptorSpec spec{};
    spec.ubos.push_back(UBODescriptorSemantic::ViewportInfo);
    spec.ubos.push_back(UBODescriptorSemantic::CameraInfo);
    return spec;
}

static ShaderBuildDescriptorSpec MakeSkyDescriptorSpec()
{
    ShaderBuildDescriptorSpec spec{};
    spec.ubos.push_back(UBODescriptorSemantic::SkyInfo);
    return spec;
}

static ShaderBuildDescriptorSpec MakeColorPaletteDescriptorSpec()
{
    ShaderBuildDescriptorSpec spec{};
    spec.ubos.push_back(UBODescriptorSemantic::ColorPalette);
    return spec;
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

static ShaderBuildDescriptorSpec MakeMaterialInstanceSchemaWithoutBytesDescriptorSpec()
{
    ShaderBuildDescriptorSpec spec{};
    spec.material_instance_schema = ShaderDataSchema::Color4f;
    spec.material_instance_bytes = 0;
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
    PrintBuildResult("MaterialInstanceRequested",result);

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
}

static void TestDescriptorParityWithLegacyForMaterialInstanceConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.SetMaterialInstance(spec.material_instance_bytes,cfg.shader_stage_flag_bit));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());
    CHECK_EQ(pipeline_result.value.material_instance.stride, legacy_mci->GetMaterialInstance().stride);
    CHECK_EQ(pipeline_result.value.material_instance.stage_bits, legacy_mci->GetMaterialInstance().stage_bits);

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestCompileSucceedsWhenMaterialInstanceSchemaSpecProvided()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaDescriptorSpec();

    auto result = pipeline.Build(cfg,&profile,&spec);
    PrintBuildResult("MaterialInstanceSchemaSpecProvided",result);

    CHECK_TRUE(result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Compiled);
    CHECK_TRUE(result.value.layout_finalized);
    CHECK_TRUE(!result.value.binaries.empty());
    CHECK_TRUE(!result.value.binaries[0].spirv.empty());
    CHECK_EQ((int)result.value.material_instance.schema, (int)spec.material_instance_schema);
    CHECK_TRUE(!result.value.material_instance.schema_file.empty());
    CHECK_TRUE(result.diagnostics[0].message=="schema-aware compile path active: schema_color4f.glsl");

    bool has_schema_diag=false;
    for(const auto &d:result.diagnostics)
    {
        if(d.subject=="ShaderBuildPipeline.MaterialInstance.Schema")
            has_schema_diag=true;
    }

    CHECK_TRUE(has_schema_diag);
}

static void TestBuildFailsWhenMaterialInstanceSchemaByteSizeMismatched()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaDescriptorSpec();
    ++spec.material_instance_bytes;

    auto result = pipeline.Build(cfg,&profile,&spec);
    PrintBuildResult("MaterialInstanceSchemaByteSizeMismatched",result);

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
    CHECK_TRUE(result.diagnostics[0].subject=="ShaderBuildPipeline.MaterialInstance.Schema");
    CHECK_TRUE(result.diagnostics[0].message=="material_instance schema byte size mismatch");
}

static void TestBuildFailsWhenMaterialInstanceSchemaBytesMissing()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaWithoutBytesDescriptorSpec();

    auto result = pipeline.Build(cfg,&profile,&spec);
    PrintBuildResult("MaterialInstanceSchemaBytesMissing",result);

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
    CHECK_TRUE(result.diagnostics[0].subject=="ShaderBuildPipeline.MaterialInstance.Schema");
    CHECK_TRUE(result.diagnostics[0].message=="material_instance schema requires non-zero byte size");
}

static void TestRouteSwitchEvaluationForSchemaAwareMaterialInstance()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaDescriptorSpec();

    auto result = pipeline.Build(cfg,&profile,&spec);
    auto evaluation = EvaluateShaderBuildResultForRouteSwitch(result);

    CHECK_TRUE(evaluation.pipeline_ready);
    CHECK_TRUE(evaluation.baseline_compare_ready);
    CHECK_TRUE(evaluation.schema_aware_material_instance);
    CHECK_TRUE(evaluation.reasons.empty());

    const std::string summary=GetShaderBuildRouteEvaluationSummary(evaluation);
    CHECK_TRUE(summary.find("pipeline_ready=true")!=std::string::npos);
    CHECK_TRUE(summary.find("baseline_compare_ready=true")!=std::string::npos);
    CHECK_TRUE(summary.find("schema_aware_material_instance=true")!=std::string::npos);

    const char *summary_file="build/shadergen_route_readiness_smoke.txt";
    CHECK_TRUE(WriteShaderBuildRouteEvaluationSummary(evaluation,summary_file));

    std::ifstream ifs(summary_file,std::ios::in);
    CHECK_TRUE(ifs.is_open());
    std::string file_text;
    std::getline(ifs,file_text);
    CHECK_TRUE(file_text.find("pipeline_ready=true")!=std::string::npos);
}

static void TestRouteSwitchEvaluationRejectsFailedSchemaBuild()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaWithoutBytesDescriptorSpec();

    auto result = pipeline.Build(cfg,&profile,&spec);
    auto evaluation = EvaluateShaderBuildResultForRouteSwitch(result);

    CHECK_TRUE(!evaluation.pipeline_ready);
    CHECK_TRUE(!evaluation.baseline_compare_ready);
    CHECK_TRUE(!evaluation.schema_aware_material_instance);
    CHECK_TRUE(!evaluation.reasons.empty());

    const std::string summary=GetShaderBuildRouteEvaluationSummary(evaluation);
    CHECK_TRUE(summary.find("pipeline_ready=false")!=std::string::npos);
    CHECK_TRUE(summary.find("baseline_compare_ready=false")!=std::string::npos);
    CHECK_TRUE(summary.find("reasons=")!=std::string::npos);
}

static void TestCompileCompositorRoutePlanDefaults()
{
    const auto plan=BuildCompileCompositorRoutePlan();

    CHECK_TRUE(plan.preferred_route==ShaderBuildRoute::LegacyMaterialCreateInfo);
    CHECK_TRUE(plan.allow_pipeline_fallback);
    CHECK_TRUE(plan.can_export_readiness);
    CHECK_TRUE(plan.can_emit_baseline_artifacts);
    CHECK_TRUE(!plan.rationale.empty());
}

static void TestCompileCompositorRouteDecisionDefaultsToLegacy()
{
    const auto decision=ResolveCompileCompositorRouteDecision(nullptr);

    CHECK_TRUE(decision.resolved_route==ShaderBuildRoute::LegacyMaterialCreateInfo);
    CHECK_TRUE(decision.will_use_legacy_now);
    CHECK_TRUE(!decision.pipeline_trial_requested);
    CHECK_TRUE(decision.fallback_to_legacy);
    CHECK_TRUE(!decision.rationale.empty());
}

static void TestCompileCompositorRouteDecisionSummary()
{
    ShaderBuildSwitchConfig switch_config{};
    switch_config.enable_pipeline=true;

    const auto decision=ResolveCompileCompositorRouteDecision(&switch_config);
    const std::string summary=GetCompileCompositorRouteDecisionSummary(decision);

    CHECK_TRUE(summary.find("resolved_route=Pipeline")!=std::string::npos);
    CHECK_TRUE(summary.find("will_use_legacy_now=true")!=std::string::npos);
    CHECK_TRUE(summary.find("pipeline_trial_requested=true")!=std::string::npos);
    CHECK_TRUE(summary.find("fallback_to_legacy=true")!=std::string::npos);
}

static void TestCompileCompositorShadowPipelineReport()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();

    const auto report = BuildCompileCompositorShadowPipelineReport(&profile,def,&cfg);

    CHECK_TRUE(report.result.success);
    CHECK_TRUE(report.evaluation.pipeline_ready);
    CHECK_TRUE(report.evaluation.baseline_compare_ready);
    CHECK_TRUE(report.evaluation.schema_aware_material_instance);
    CHECK_TRUE(report.summary.find("pipeline_ready=true")!=std::string::npos);
    CHECK_TRUE(report.summary.find("schema_aware_material_instance=true")!=std::string::npos);
}

static void TestCompileCompositorShadowBuildArtifacts()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    const auto report = BuildCompileCompositorShadowPipelineReport(&profile,def,&cfg);

    const char *reports_dir = "build/shadergen_trial/reports";
    CHECK_TRUE(WriteCompileCompositorShadowBuildArtifacts(report,def.name,reports_dir));

    std::ifstream readiness_ifs("build/shadergen_trial/reports/SchemaAwareSmokeCompositor_readiness.txt",std::ios::in);
    CHECK_TRUE(readiness_ifs.is_open());
    std::string readiness_text;
    std::getline(readiness_ifs,readiness_text);
    CHECK_TRUE(readiness_text.find("pipeline_ready=true")!=std::string::npos);

    std::ifstream diagnostics_ifs("build/shadergen_trial/reports/SchemaAwareSmokeCompositor_diagnostics.log",std::ios::in);
    CHECK_TRUE(diagnostics_ifs.is_open());
    std::string diagnostics_text;
    std::getline(diagnostics_ifs,diagnostics_text);
    CHECK_TRUE(diagnostics_text.find("material=SchemaAwareSmokeCompositor")!=std::string::npos);
}

static void TestCompileCompositorTrialBaselineReport()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    const auto report = BuildCompileCompositorShadowPipelineReport(&profile,def,&cfg);

    CHECK_TRUE(WriteCompileCompositorTrialBaselineReport(report,
                                                         def.name,
                                                         true,
                                                         "legacy compile succeeded",
                                                         "build/shadergen_trial"));

    std::ifstream report_ifs("build/shadergen_trial/reports/SchemaAwareSmokeCompositor_baseline_compare.md",std::ios::in);
    CHECK_TRUE(report_ifs.is_open());

    std::string report_text((std::istreambuf_iterator<char>(report_ifs)),
                            std::istreambuf_iterator<char>());
    CHECK_TRUE(report_text.find("# ShaderGen 基线对比报告（自动生成）")!=std::string::npos);
    CHECK_TRUE(report_text.find("Readiness: `pipeline_ready=true")!=std::string::npos);
    CHECK_TRUE(report_text.find("LegacySummary: `legacy compile succeeded`")!=std::string::npos);
}

static void TestCompileCompositorShadowPipelineTree()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();
    const auto report = BuildCompileCompositorShadowPipelineReport(&profile,def,&cfg);

    CHECK_TRUE(WriteCompileCompositorShadowPipelineTree(report,
                                                        def.name,
                                                        "build/shadergen_trial/pipeline"));

    std::ifstream descriptor_ifs("build/shadergen_trial/pipeline/SchemaAwareSmokeCompositor/descriptor_spec.txt",std::ios::in);
    CHECK_TRUE(descriptor_ifs.is_open());
    std::string descriptor_text;
    std::getline(descriptor_ifs,descriptor_text);
    CHECK_TRUE(descriptor_text.find("ubos=")!=std::string::npos);

    std::ifstream result_ifs("build/shadergen_trial/pipeline/SchemaAwareSmokeCompositor/result_summary.txt",std::ios::in);
    CHECK_TRUE(result_ifs.is_open());
    std::string result_text;
    std::getline(result_ifs,result_text);
    CHECK_TRUE(result_text.find("success=true")!=std::string::npos);

    std::ifstream spv_ifs("build/shadergen_trial/pipeline/SchemaAwareSmokeCompositor/stage_0.spv.txt",std::ios::in);
    CHECK_TRUE(spv_ifs.is_open());
}

static void TestCompileCompositorLegacyTree()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();

    MaterialCreateInfo *mci = CompileCompositorMaterial(&profile,
                                                        def,
                                                        "#version 450\nvoid main(){}\n",
                                                        "#version 450\nlayout(location=0) out vec4 outColor; void main(){outColor=vec4(1.0);}\n",
                                                        &cfg);
    CHECK_TRUE(mci!=nullptr);

    if(!mci)
        return;

    CHECK_TRUE(WriteCompileCompositorLegacyTree(*mci,def.name,"build/shadergen_trial/legacy"));

    std::ifstream descriptor_ifs("build/shadergen_trial/legacy/SchemaAwareSmokeCompositor/descriptor_info.txt",std::ios::in);
    CHECK_TRUE(descriptor_ifs.is_open());

    std::ifstream vertex_glsl_ifs("build/shadergen_trial/legacy/SchemaAwareSmokeCompositor/vertex.glsl",std::ios::in);
    CHECK_TRUE(vertex_glsl_ifs.is_open());

    std::ifstream fragment_spv_ifs("build/shadergen_trial/legacy/SchemaAwareSmokeCompositor/fragment.spv.txt",std::ios::in);
    CHECK_TRUE(fragment_spv_ifs.is_open());

    delete mci;
}

static void TestCompileCompositorBaselineCompareCommand()
{
    const std::string command=BuildCompileCompositorBaselineCompareCommand("SchemaAwareSmokeCompositor","build/shadergen_trial");

    CHECK_TRUE(command.find("python shadergen_baseline_compare.py")!=std::string::npos);
    CHECK_TRUE(command.find("SchemaAwareSmokeCompositor")!=std::string::npos);
    CHECK_TRUE(command.find("legacy")!=std::string::npos);
    CHECK_TRUE(command.find("pipeline")!=std::string::npos);
    CHECK_TRUE(command.find("_readiness.txt")!=std::string::npos);
}

static void TestCompileCompositorTrialAggregateReport()
{
    CHECK_TRUE(WriteCompileCompositorTrialAggregateReport("build/shadergen_trial"));

    std::ifstream aggregate_ifs("build/shadergen_trial/reports/baseline_compare.md",std::ios::in);
    CHECK_TRUE(aggregate_ifs.is_open());

    std::string aggregate_text((std::istreambuf_iterator<char>(aggregate_ifs)),
                               std::istreambuf_iterator<char>());
    CHECK_TRUE(aggregate_text.find("# ShaderGen 试运行汇总报告（自动生成）")!=std::string::npos);
    CHECK_TRUE(aggregate_text.find("SchemaAwareSmokeCompositor_baseline_compare.md")!=std::string::npos);
}

static void TestCompileCompositorTrialBatchSummary()
{
    CompileCompositorTrialBatchReport report{};
    report.total_count = 2;
    report.legacy_success_count = 1;
    report.pipeline_trial_success_count = 2;
    report.baseline_report_count = 1;
    report.baseline_compare_success_count = 1;
    report.aggregate_report_written = true;

    const std::string summary = GetCompileCompositorTrialBatchSummary(report);
    CHECK_TRUE(summary.find("total_count=2")!=std::string::npos);
    CHECK_TRUE(summary.find("legacy_success_count=1")!=std::string::npos);
    CHECK_TRUE(summary.find("aggregate_report_written=true")!=std::string::npos);
}

static void TestCompileCompositorTrialBatch()
{
    Material3DCreateConfig cfg(PrimitiveType::Triangles,
                               IncludeCamera::With,
                               IncludeL2W::With,
                               IncludeSky::Without);
    cfg.material_instance = true;
    cfg.shader_stage_flag_bit = uint32_t(ShaderStage::VertexFragment);

    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const StaticMaterialDef def = MakeSchemaAwareCompositorDef();

    CompileCompositorTrialBatchItem item{};
    item.def = &def;
    item.vs_glsl = "#version 450\nvoid main(){}\n";
    item.fs_glsl = "#version 450\nlayout(location=0) out vec4 outColor; void main(){outColor=vec4(1.0);}\n";
    item.config = &cfg;
    item.material_name_override = "SchemaAwareSmokeBatch";

    std::vector<CompileCompositorTrialBatchItem> items;
    items.push_back(item);

    const auto report = RunCompileCompositorTrialBatch(&profile,
                                                       items,
                                                       "build/shadergen_trial",
                                                       false);

    CHECK_EQ(report.total_count, size_t(1));
    CHECK_EQ(report.legacy_success_count, size_t(1));
    CHECK_EQ(report.pipeline_trial_success_count, size_t(1));
    CHECK_EQ(report.baseline_report_count, size_t(1));
    CHECK_EQ(report.baseline_compare_success_count, size_t(0));
    CHECK_TRUE(report.aggregate_report_written);

    std::ifstream pipeline_ifs("build/shadergen_trial/pipeline/SchemaAwareSmokeBatch/result_summary.txt",std::ios::in);
    CHECK_TRUE(pipeline_ifs.is_open());

    std::ifstream legacy_ifs("build/shadergen_trial/legacy/SchemaAwareSmokeBatch/descriptor_info.txt",std::ios::in);
    CHECK_TRUE(legacy_ifs.is_open());

    std::ifstream batch_report_ifs("build/shadergen_trial/reports/SchemaAwareSmokeBatch_baseline_compare.md",std::ios::in);
    CHECK_TRUE(batch_report_ifs.is_open());
}

static void TestCompileCompositorBuiltinCandidateTrialBatchSummary()
{
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    const auto report = RunCompileCompositorBuiltinCandidateTrialBatch(&profile,
                                                                       "build/shadergen_trial",
                                                                       false);

    CHECK_TRUE(report.total_count >= size_t(2));
    CHECK_TRUE(report.pipeline_trial_success_count >= size_t(2));
    CHECK_TRUE(report.aggregate_report_written);

    const std::string summary = GetCompileCompositorTrialBatchSummary(report);
    CHECK_TRUE(summary.find("total_count=")!=std::string::npos);

    std::ifstream gizmo_report_ifs("build/shadergen_trial/reports/Gizmo3D_baseline_compare.md",std::ios::in);
    CHECK_TRUE(gizmo_report_ifs.is_open());

    std::ifstream billboard_dynamic_report_ifs("build/shadergen_trial/reports/BillboardDynamic_baseline_compare.md",std::ios::in);
    CHECK_TRUE(billboard_dynamic_report_ifs.is_open());

    std::ifstream pure_color_2d_report_ifs("build/shadergen_trial/reports/PureColor2D_baseline_compare.md",std::ios::in);
    CHECK_TRUE(pure_color_2d_report_ifs.is_open());

    std::ifstream vertex_color_3d_report_ifs("build/shadergen_trial/reports/VertexColor3D_baseline_compare.md",std::ios::in);
    CHECK_TRUE(vertex_color_3d_report_ifs.is_open());

    std::ifstream text2d_report_ifs("build/shadergen_trial/reports/Text2D_baseline_compare.md",std::ios::in);
    CHECK_TRUE(text2d_report_ifs.is_open());

    std::ifstream standard_pipeline_ifs("build/shadergen_trial/pipeline/Standard_v1/result_summary.txt",std::ios::in);
    CHECK_TRUE(standard_pipeline_ifs.is_open());
}

static void TestCompileCompositorRouteDecisionKeepsLegacyWhenPipelineRequested()
{
    ShaderBuildSwitchConfig switch_config{};
    switch_config.enable_pipeline=true;

    const auto decision=ResolveCompileCompositorRouteDecision(&switch_config);

    CHECK_TRUE(decision.resolved_route==ShaderBuildRoute::Pipeline);
    CHECK_TRUE(decision.will_use_legacy_now);
    CHECK_TRUE(decision.pipeline_trial_requested);
    CHECK_TRUE(decision.fallback_to_legacy);
    CHECK_TRUE(!decision.rationale.empty());
}

static void TestBuildModelParityWithLegacyForMaterialInstanceSchemaConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeMaterialInstanceSchemaDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    const auto &schema_info=GetShaderDataSchemaInfo(spec.material_instance_schema);
    CHECK_TRUE(builder.SetMaterialInstance(spec.material_instance_schema,schema_info,cfg.shader_stage_flag_bit));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.material_instance.stride, legacy_mci->GetMaterialInstance().stride);
    CHECK_EQ(pipeline_result.value.material_instance.stage_bits, legacy_mci->GetMaterialInstance().stage_bits);
    CHECK_EQ((int)pipeline_result.value.material_instance.schema, (int)legacy_mci->GetMaterialInstance().schema);
    CHECK_TRUE(pipeline_result.value.material_instance.schema_file == legacy_mci->GetMaterialInstance().schema_file);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForTextureSamplerOverrideConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeTextureSamplerOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto pipeline_result = pipeline.Build(cfg,&profile);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddTextureSampler(ShaderStage::Fragment,SamplerType::Sampler2D,SamplerSlot::BaseColor));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());
    CHECK_EQ(pipeline_result.value.local_to_world.enabled, legacy_mci->GetLocalToWorld().enabled);
    CHECK_EQ(pipeline_result.value.local_to_world.stage_bits, legacy_mci->GetLocalToWorld().stage_bits);

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForViewportCameraAndLocalToWorldConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeLocalToWorldConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeViewportCameraDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::ViewportInfo));
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::CameraInfo));
    CHECK_TRUE(builder.SetLocalToWorld(cfg.shader_stage_flag_bit));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForViewportCameraAndTextureSamplerConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeVertexFragmentTextureSamplerOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeViewportCameraDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::ViewportInfo));
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::CameraInfo));
    CHECK_TRUE(builder.AddTextureSampler(ShaderStage::Fragment,SamplerType::Sampler2D,SamplerSlot::BaseColor));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForViewportCameraLocalToWorldAndTextureSamplerConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeVertexFragmentLocalToWorldTextureSamplerOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeViewportCameraDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::ViewportInfo));
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::CameraInfo));
    CHECK_TRUE(builder.SetLocalToWorld(cfg.shader_stage_flag_bit));
    CHECK_TRUE(builder.AddTextureSampler(ShaderStage::Fragment,SamplerType::Sampler2D,SamplerSlot::BaseColor));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForSkyUBOFragmentConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeFragmentConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeSkyDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::SkyInfo));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForColorPaletteUBOFragmentConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeFragmentConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeColorPaletteDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::ColorPalette));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForViewportCameraUBOConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeBasicConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeViewportCameraDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::ViewportInfo));
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::CameraInfo));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForViewportCameraUBOFragmentConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeFragmentConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();
    ShaderBuildDescriptorSpec spec = MakeViewportCameraDescriptorSpec();

    auto pipeline_result = pipeline.Build(cfg,&profile,&spec);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::ViewportInfo));
    CHECK_TRUE(builder.AddUBOStruct(cfg.shader_stage_flag_bit,UBODescriptorSemantic::CameraInfo));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForVertexFragmentTextureSamplerOverrideConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeVertexFragmentTextureSamplerOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto pipeline_result = pipeline.Build(cfg,&profile);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddTextureSampler(ShaderStage::Fragment,SamplerType::Sampler2D,SamplerSlot::BaseColor));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestDescriptorCountParityWithLegacyForTextureSamplerMultiSlotOverrideConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeTextureSamplerMultiSlotOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto pipeline_result = pipeline.Build(cfg,&profile);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.AddTextureSampler(ShaderStage::Fragment,SamplerType::Sampler2D,SamplerSlot::BaseColor));
    CHECK_TRUE(builder.AddTextureSampler(ShaderStage::Fragment,SamplerType::Sampler2D,SamplerSlot::Normal));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    delete legacy_mci;
}

static void TestDescriptorParityWithLegacyForMinimalConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeBasicConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto pipeline_result = pipeline.Build(cfg,&profile);
    CHECK_TRUE(pipeline_result.success);

    MaterialCreateInfo legacy_mci(&cfg);

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci.GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci.GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);
}

static void TestDescriptorParityWithLegacyForFragmentConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeFragmentConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto pipeline_result = pipeline.Build(cfg,&profile);
    CHECK_TRUE(pipeline_result.success);

    MaterialCreateInfo legacy_mci(&cfg);

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci.GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci.GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);
}

static void TestBuildFailsWhenMaterialInstanceRequested()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeMaterialInstanceConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("MaterialInstanceRequested",result);

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
}

static void TestBuildFailsWhenLocalToWorldRequested()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeLocalToWorldConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("LocalToWorldRequested",result);

    CHECK_TRUE(result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Compiled);
    CHECK_TRUE(result.value.layout_finalized);
}

static void TestDescriptorParityWithLegacyForLocalToWorldConfig()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeLocalToWorldConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto pipeline_result = pipeline.Build(cfg,&profile);
    CHECK_TRUE(pipeline_result.success);

    MaterialBuilder builder(&cfg);
    CHECK_TRUE(builder.SetLocalToWorld(cfg.shader_stage_flag_bit));

    MaterialCreateInfo *legacy_mci=builder.BuildSnapshotOnly();
    CHECK_TRUE(legacy_mci!=nullptr);

    if(!legacy_mci)
        return;

    CHECK_EQ(pipeline_result.value.descriptor_count, legacy_mci->GetDescriptorInfo().GetCount());

    const auto &legacy_contract=legacy_mci->GetBindingContract();

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ubos[i], legacy_contract.ubos[i]);

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
        CHECK_EQ(pipeline_result.value.binding_contract.ssbos[i], legacy_contract.ssbos[i]);

    delete legacy_mci;
}

static void TestBuildFailsWhenTextureSamplerOverrideRequested()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeTextureSamplerOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);
    PrintBuildResult("TextureSamplerOverrideRequested",result);

    CHECK_TRUE(result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Compiled);
    CHECK_TRUE(result.value.layout_finalized);
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
    TestBuildFailsWhenMaterialInstanceRequested();
    TestBuildFailsWhenLocalToWorldRequested();
    TestBuildFailsWhenTextureSamplerOverrideRequested();
    TestDescriptorParityWithLegacyForMaterialInstanceConfig();
    TestBuildModelParityWithLegacyForMaterialInstanceSchemaConfig();
    TestCompileSucceedsWhenMaterialInstanceSchemaSpecProvided();
    TestBuildFailsWhenMaterialInstanceSchemaByteSizeMismatched();
    TestBuildFailsWhenMaterialInstanceSchemaBytesMissing();
    TestRouteSwitchEvaluationForSchemaAwareMaterialInstance();
    TestRouteSwitchEvaluationRejectsFailedSchemaBuild();
    TestCompileCompositorRoutePlanDefaults();
    TestCompileCompositorRouteDecisionDefaultsToLegacy();
    TestCompileCompositorRouteDecisionKeepsLegacyWhenPipelineRequested();
    TestCompileCompositorRouteDecisionSummary();
    TestCompileCompositorShadowPipelineReport();
    TestCompileCompositorShadowBuildArtifacts();
    TestCompileCompositorTrialBaselineReport();
    TestCompileCompositorShadowPipelineTree();
    TestCompileCompositorLegacyTree();
    TestCompileCompositorBaselineCompareCommand();
    TestCompileCompositorTrialAggregateReport();
    TestCompileCompositorTrialBatchSummary();
    TestCompileCompositorTrialBatch();
    TestCompileCompositorBuiltinCandidateTrialBatchSummary();
    TestDescriptorParityWithLegacyForMinimalConfig();
    TestDescriptorParityWithLegacyForFragmentConfig();
    TestDescriptorParityWithLegacyForTextureSamplerOverrideConfig();
    TestDescriptorCountParityWithLegacyForTextureSamplerMultiSlotOverrideConfig();
    TestDescriptorParityWithLegacyForVertexFragmentTextureSamplerOverrideConfig();
    TestDescriptorParityWithLegacyForLocalToWorldConfig();
    TestDescriptorParityWithLegacyForViewportCameraUBOConfig();
    TestDescriptorParityWithLegacyForViewportCameraUBOFragmentConfig();
    TestDescriptorParityWithLegacyForSkyUBOFragmentConfig();
    TestDescriptorParityWithLegacyForColorPaletteUBOFragmentConfig();
    TestDescriptorParityWithLegacyForViewportCameraAndLocalToWorldConfig();
    TestDescriptorParityWithLegacyForViewportCameraAndTextureSamplerConfig();
    TestDescriptorParityWithLegacyForViewportCameraLocalToWorldAndTextureSamplerConfig();

    if(g_failures==0)
        std::fprintf(stdout,"ShaderBuildPipelineSmokeTests PASSED.\n");
    else
        std::fprintf(stderr,"ShaderBuildPipelineSmokeTests FAILED: %d\n",g_failures);

    hgl::graph::CloseShaderCompiler();

    return g_failures;
}
