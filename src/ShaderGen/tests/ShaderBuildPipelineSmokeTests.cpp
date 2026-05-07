#include <hgl/shadergen/ShaderBuildPipeline.h>
#include <hgl/shadergen/MaterialCreateInfo.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>

#include "GLSLCompiler.h"

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

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))

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

static PhysicalDeviceProfileLite MakeBasicProfile()
{
    PhysicalDeviceProfileLite profile{};
    profile.name = "SmokeProfile";
    profile.target_vulkan_version = 0;
    profile.target_spv_version = 0;
    return profile;
}

static void TestBuildFailsWhenStageBitsIsZero()
{
    ShaderBuildPipeline pipeline;

    MaterialCreateConfig cfg(PrimitiveType::Triangles,false);
    cfg.shader_stage_flag_bit = 0;

    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);

    CHECK_TRUE(!result.success);
    CHECK_TRUE(!result.diagnostics.empty());
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
}

static void TestBuildFailsWhenProfileNull()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeBasicConfig();

    auto result = pipeline.Build(cfg,nullptr);

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

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
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

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
}

static void TestBuildFailsWhenTextureSamplerOverrideRequested()
{
    ShaderBuildPipeline pipeline;
    MaterialCreateConfig cfg = MakeTextureSamplerOverrideConfig();
    PhysicalDeviceProfileLite profile = MakeBasicProfile();

    auto result = pipeline.Build(cfg,&profile);

    CHECK_TRUE(!result.success);
    CHECK_EQ(result.value.final_state, ShaderBuildState::Failed);
    CHECK_TRUE(!result.diagnostics.empty());
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
    TestDescriptorParityWithLegacyForMinimalConfig();
    TestDescriptorParityWithLegacyForFragmentConfig();

    if(g_failures==0)
        std::fprintf(stdout,"ShaderBuildPipelineSmokeTests PASSED.\n");
    else
        std::fprintf(stderr,"ShaderBuildPipelineSmokeTests FAILED: %d\n",g_failures);

    hgl::graph::CloseShaderCompiler();

    return g_failures;
}
