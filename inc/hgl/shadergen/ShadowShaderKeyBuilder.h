#pragma once

#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/mtl/MaterialProgramContract.h>
#include <hgl/shadergen/ShaderArtifactContract.h>
#include <hgl/shadergen/ShaderProgramKey.h>
#include <hgl/shadergen/ShaderStageKey.h>
#include <hgl/shadergen/ShadowShaderContractBuilder.h>

namespace hgl::graph::mtl
{
    enum class ShadowShaderKeyBuildError : uint8
    {
        None = 0,
        InvalidModuleGraph,
        InvalidShaderContract,
        MissingLegacyStage,
        InvalidSelectionKey,
        InvalidEffectiveProgramKey,
        InvalidShaderVariant,
        InvalidStageKey,
        InvalidProgramKey,
        InvalidProgramMetadata
    };

    struct ShadowShaderKeyBuildDiagnostic
    {
        ShadowShaderKeyBuildError error =
            ShadowShaderKeyBuildError::None;
        AnsiString detail;
    };

    struct ShadowShaderKeys
    {
        MaterialSelectionRequestKey selection_request;
        EffectiveMaterialProgramKey effective_program;
        ShaderVariantContract shader_variant;
        ShaderStageKey vertex_stage;
        ShaderStageKey fragment_stage;
        ShaderProgramKey program;
        ShaderProgramArtifactMetadata program_metadata;
    };

    const char *GetShadowShaderKeyBuildErrorName(
        ShadowShaderKeyBuildError error) noexcept;

    bool ValidateShaderProgramArtifactMetadata(
        const ShaderProgramArtifactMetadata &metadata) noexcept;
    uint64 GetShaderProgramArtifactMetadataHash(
        const ShaderProgramArtifactMetadata &metadata) noexcept;

    bool BuildShadowShaderKeys(
        const contract::PhysicalDeviceProfileLite *profile,
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const ResolvedModuleGraph &module_graph,
        const ShadowShaderContracts &contracts,
        const ShaderProgramBuildSpec &legacy_build_spec,
        ShadowShaderKeys &out_keys,
        ShadowShaderKeyBuildDiagnostic &out_diagnostic);
}
