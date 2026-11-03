// 门禁入口：组过滤调度与发行夹具烘焙模式。case 实现见 Gate*.cpp（阶段 5 拆分）。
#include "GateTestSupport.h"

using namespace gate;

int main(const int argc, char **argv)
{
    // 发行形态夹具烘焙模式：argc==3 且 argv[1]=="emit-release-cache"。
    // 产物（manifest + 两 stage SPV + expectation.txt）留给 RuntimeFormProbe
    // 在 ULRE_ENABLE_SHADER_PROGRAM_BUILD=0 构建里消费。
    if (argc == 3 && std::strcmp(argv[1], "emit-release-cache") == 0)
    {
        if (!InitShaderCompiler())
        {
            GLogError(
                "[ShaderResourceSchemaRegressionGate] Failed to initialize GLSL compiler.");
            return 3;
        }

        const std::filesystem::path cache_root =
            std::filesystem::absolute(std::filesystem::path(argv[2]));
        GateResult manifest_result = RunProgramManifestLookupCase(cache_root, true);

        std::fprintf(manifest_result.passed ? stdout : stderr,
                     "[%s] %s\n",
                     manifest_result.passed ? "PASS" : "FAIL",
                     manifest_result.name.c_str());

        for (const auto &diag : manifest_result.diagnostics)
            std::fprintf(stderr, "  - %s\n", diag.c_str());

        // compute 夹具烘焙进同一个根：材质清单与 compute 清单是两份文件，
        // 互不覆盖，发行形态一次打包全部带走。
        if (manifest_result.passed)
        {
            GateResult compute_result = RunComputeProgramCacheCase(cache_root, true);

            std::fprintf(compute_result.passed ? stdout : stderr,
                         "[%s] %s\n",
                         compute_result.passed ? "PASS" : "FAIL",
                         compute_result.name.c_str());

            for (const auto &diag : compute_result.diagnostics)
                std::fprintf(stderr, "  - %s\n", diag.c_str());

            if (!compute_result.passed)
                return 1;
        }

        if (manifest_result.passed)
        {
            hgl::OSString cache_dir;
            if (ResolveEmittedCacheDir(cache_root, cache_dir))
                std::fprintf(stdout, "[INFO] release cache emitted to %s\n",
                             std::filesystem::path(cache_dir.c_str())
                                 .string().c_str());
            else
                std::fprintf(stdout, "[INFO] release cache emitted under %s\n",
                             cache_root.string().c_str());
        }

        return manifest_result.passed ? 0 : 1;
    }

    if (argc > 2)
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Expected zero or one group argument.");
        return 2;
    }

    const char *selected_group = argc == 2 ? argv[1] : "all";

    // `emit-fixed-mesh` 不是回归组，而是覆盖率探针：一次跑完三张矩阵，
    // 逐形状点名 `ShaderLibrary/topology` 下有没有对应的形状 master。
    // 它与门禁共用同一张变体表，形状定义只有一份——覆盖检查与验证不可能互相漂移。
    if (std::strcmp(selected_group, "emit-fixed-mesh") == 0)
    {
        if (!InitShaderCompiler())
        {
            GLogError(
                "[ShaderResourceSchemaRegressionGate] Failed to initialize GLSL compiler.");
            return 3;
        }
        return EmitFixedMeshCatalog();
    }

    if (!IsKnownRegressionGroup(selected_group))
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Unknown regression group: %s",
            selected_group);
        return 2;
    }

    if (!InitShaderCompiler())
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Failed to initialize GLSL compiler.");
        return 3;
    }

    const bool run_glsl = IsRegressionGroupSelected(selected_group, "glsl");
    const bool run_interface = IsRegressionGroupSelected(selected_group, "interface");
    const bool run_descriptor = IsRegressionGroupSelected(selected_group, "descriptor");
    const bool run_cache = IsRegressionGroupSelected(selected_group, "cache");
    const bool run_materialization =
        IsRegressionGroupSelected(selected_group, "materialization");
    const bool run_pipeline = IsRegressionGroupSelected(selected_group, "pipeline");
    const bool run_reflection = IsRegressionGroupSelected(selected_group, "reflection");
    const bool run_module_invariants =
        IsRegressionGroupSelected(selected_group, "module-invariants");

    std::vector<GateResult> results;

    if (run_descriptor)
    {
    }

    if (run_materialization) results.push_back(RunMaterializationSharedInstanceCase());
    if (run_materialization) results.push_back(RunMaterialDomainCoverageCase());
    if (run_materialization) results.push_back(RunMaterialIdentityCase());
    if (run_materialization) results.push_back(RunMaterialFallbackDefinitionCase());
    if (run_pipeline) results.push_back(RunUnifiedMaterialBaselineCase());
    if (run_pipeline) results.push_back(RunMaterialOutputContractCase());
    if (run_pipeline) results.push_back(RunPrimitiveVariantPurposeCase());
    if (run_pipeline) results.push_back(RunUnifiedForwardSkeletonCase());
    if (run_pipeline) results.push_back(RunUnifiedPureColorFragmentCase());
    if (run_descriptor) results.push_back(RunUnifiedMaterialContractCase());
    if (run_materialization) results.push_back(RunTransformGraphModelCase());
    if (run_materialization) results.push_back(RunTransformGraphCompositionCase());
    if (run_cache) results.push_back(RunAuthoritativeMaterialCacheIdentityCase());
    if (run_descriptor) results.push_back(RunGlobalSSBOBindingKeyCase());
    if (run_materialization) results.push_back(RunResolvedMaterialRenderStateCase());
    if (run_materialization) results.push_back(RunMaterialVertexABIResolutionCase());
    if (run_materialization) results.push_back(RunMaterialRecipeParseDetailCase());
    if (run_materialization) results.push_back(RunMaterialDomainContractCase());
    if (run_materialization) results.push_back(RunMaterialLibraryContractCase());
    if (run_materialization) results.push_back(RunFallbackInferenceCase());
    if (run_glsl) results.push_back(RunProviderResourceManifestCase());
    if (run_glsl) results.push_back(RunGlslModuleFileCase());
    if (run_glsl) results.push_back(RunGlslModuleMetadataValidationCase());
    if (run_interface) results.push_back(RunShaderSemanticRegistryCase());
    if (run_interface) results.push_back(RunDepthVaryingMaskParityCase());
    if (run_interface) results.push_back(RunCharQuadWorkgroupContractCase());
    if (run_interface) results.push_back(RunDrawTriangleVertexColor2DCase());
    if (run_interface) results.push_back(RunMaterialVertexABICharacterizationCase());
    if (run_interface) results.push_back(RunMaterialSemanticABIParityCase());
    if (run_interface) results.push_back(RunGlobalAddressesStructParityCase());
    if (run_interface) results.push_back(RunWorldAddressesStructParityCase());
    if (run_interface) results.push_back(RunEntityGPUStructParityCase());
    if (run_interface) results.push_back(RunLightRowStructParityCase());
    if (run_interface) results.push_back(RunBatchInfoStructParityCase());
    if (run_interface) results.push_back(RunMeshDrawParamsHeadParityCase());
    if (run_interface) results.push_back(RunAddressOwnershipCase(false));
    if (run_interface) results.push_back(RunAddressOwnershipCase(true));
    if (run_glsl) results.push_back(RunVkEnumTextCase());
    if (run_cache) results.push_back(RunComposedStageInterfaceCase());
    if (run_cache) results.push_back(RunResolvedStageCacheIdentityCase());
    if (run_cache) results.push_back(RunCanonicalShaderContractCase());
    if (run_cache) results.push_back(RunProgramManifestCase());
    if (run_cache) results.push_back(
        RunProgramManifestLookupCase(
            std::filesystem::path(RepoRootOSPath("build/cache-manifest-lookup").c_str())));
    if (run_cache) results.push_back(
        RunComputeProgramCacheCase(
            std::filesystem::path(RepoRootOSPath("build/cache-compute-lookup").c_str())));
    if (run_pipeline) results.push_back(RunMaterialBDAFixedBindingsCase());
    if (run_pipeline) results.push_back(RunBindingMacroSingleSourceCase());
    if (run_pipeline) results.push_back(RunShaderLibraryPathCase());
    if (run_reflection) results.push_back(RunSPVReflectionCase());
    if (run_reflection) results.push_back(RunSPVBufferReferenceReflectionCase());
    if (run_reflection) results.push_back(RunGeneratedInterfaceReflectionCase());
    if (run_materialization) results.push_back(RunSamplerPresetLibraryCase());
    if (run_interface) results.push_back(RunFixedMeshVaryingInvariantsCase());
    if (run_module_invariants) results.push_back(RunModuleInvariantsCase());
    if (run_module_invariants) results.push_back(RunSamplerReferenceContractCase());
    if (run_module_invariants) results.push_back(RunModuleIncludeDeclarationContractCase());
    if (run_pipeline) results.push_back(RunDomainBackendEquivalenceCase());
    if (run_pipeline) results.push_back(RunDomainBackendCacheCase());

    bool all_passed = true;
    for (const auto &result : results)
    {
        std::fprintf(result.passed ? stdout : stderr,
                     "[%s] %s\n",
                     result.passed ? "PASS" : "FAIL",
                     result.name.c_str());

        if (!result.passed)
        {
            all_passed = false;
            for (const auto &diag : result.diagnostics)
                std::fprintf(stderr, "  - %s\n", diag.c_str());
        }
    }

    return all_passed ? 0 : 1;
}
