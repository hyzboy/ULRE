#pragma once

#include <hgl/mtl/MaterialLibrary.h>
#include <hgl/shadergen/CanonicalShaderContract.h>
#include <hgl/shadergen/ShaderProgramBuildSpec.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    enum class ShadowShaderContractBuildError : uint8
    {
        None = 0,
        InvalidModuleGraph,
        MissingLegacyStage,
        InvalidGeometryInput,
        GeometryMismatch,
        MissingVaryingDeclaration,
        VaryingCountMismatch,
        DescriptorMismatch,
        InvalidOutputDeclaration,
        PurposeOutputMismatch,
        InvalidCanonicalContract
    };

    struct ShadowShaderContractBuildDiagnostic
    {
        ShadowShaderContractBuildError error =
            ShadowShaderContractBuildError::None;
        AnsiString detail;
        VertexSemantic geometry_semantic = VertexSemantic::Unknown;
        InterStageSemantic inter_stage_semantic =
            InterStageSemantic::Unknown;
        DescriptorSemantic descriptor_semantic =
            DescriptorSemantic::Unknown;
    };

    struct ShadowShaderContracts
    {
        ShaderInterfaceContract shader_interface;
        OutputContract output;
    };

    const char *GetShadowShaderContractBuildErrorName(
        ShadowShaderContractBuildError error) noexcept;

    bool BuildShadowShaderContracts(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const ResolvedModuleGraph &module_graph,
        const ShaderProgramBuildSpec &legacy_build_spec,
        ShadowShaderContracts &out_contracts,
        ShadowShaderContractBuildDiagnostic &out_diagnostic);
}
