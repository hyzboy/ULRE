#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    enum class MaterialStageInterfaceError : uint8
    {
        None = 0,
        InvalidVaryingConfiguration,
        MissingSemanticMetadata,
        InvalidContract
    };

    struct MaterialStageInterfaceDiagnostic
    {
        MaterialStageInterfaceError error =
            MaterialStageInterfaceError::None;
        InterStageSemantic semantic = InterStageSemantic::Unknown;
    };

    const char *GetMaterialStageInterfaceErrorName(
        MaterialStageInterfaceError error) noexcept;

    InterStageSemanticMask GetMaterialInterStageSemanticMask(
        const mtl::MaterialVertexVaryingConfig &varying) noexcept;

    bool BuildMaterialStageInterface(
        const mtl::MaterialVertexVaryingConfig &varying,
        ValueArray<InterStageSemanticContractEntry> &out_entries,
        MaterialStageInterfaceDiagnostic &out_diagnostic) noexcept;

    const InterStageSemanticContractEntry *FindMaterialStageInterfaceEntry(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        InterStageSemantic semantic) noexcept;

    bool BuildGLSLInterStageDeclaration(
        const InterStageSemanticContractEntry &entry,
        const char *direction,
        AnsiString &out_declaration);

    bool BuildGLSLMaterialSurfaceInput(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        bool camera_ubo_available,
        AnsiString &out_code);
}
