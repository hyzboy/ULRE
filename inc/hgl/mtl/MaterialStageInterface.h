#pragma once

namespace hgl::graph::mtl {}

#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/ShaderDocument.h>
#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    // 契约错误 X 列表（单一真源——枚举与 GetXxxErrorName 同源，新增错误只改此处）
#define HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST \
    HGL_ERROR(None) \
    HGL_ERROR(InvalidVaryingConfiguration) \
    HGL_ERROR(MissingSemanticMetadata) \
    HGL_ERROR(InvalidContract)

    enum class MaterialStageInterfaceError : uint8
    {
#define HGL_ERROR(name) name,
        HGL_MATERIAL_STAGE_INTERFACE_ERROR_LIST
#undef HGL_ERROR
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

    bool BuildStageInterfaceDocument(
        const ValueArray<InterStageSemanticContractEntry> &entries,
        const char *direction,
        const char *stage,
        ShaderDocument &out_document);

    // 该语义在 mesh shader 里是 per-primitive（perprimitiveEXT）而非 per-vertex：
    // 每图元恒定一份（DataIndexID/StyleID——flat 且按图元共享）。
    // mesh 侧 out 数组尺寸 = max_primitives，FS 侧 in 必须同加 perprimitiveEXT。
    inline bool IsPerPrimitiveInterStageSemantic(
        const InterStageSemantic semantic) noexcept
    {
        return semantic == InterStageSemantic::DataIndexID
            || semantic == InterStageSemantic::StyleID;
    }
}
