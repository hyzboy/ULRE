#pragma once

#include <hgl/graph/glsl/GLSLCodeModuleRegistry.h>
#include <hgl/mtl/MaterialProgramContract.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterializationSpec.h>
#include <hgl/shadergen/ResolvedModuleGraphBuilder.h>
#include <hgl/shadergen/ShadowShaderContractBuilder.h>

namespace hgl::graph::mtl
{
    enum class ShadowResourcePlanError : uint8
    {
        None = 0,
        InvalidEffectiveProgram,
        InvalidShaderInterface,
        DuplicateRecipeTexture,
        DuplicateRecipeSSBO,
        DuplicatePlanResource,
        InvalidPlan,
        MaterializationTextureMismatch,
        MaterializationStructMismatch
    };

    struct ShadowResourcePlanDiagnostic
    {
        ShadowResourcePlanError error = ShadowResourcePlanError::None;
        uint64 logical_resource_id = 0;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32 data_slot = 0;
        SSBOType ssbo_type = SSBOType::UserDefined;
    };

    struct ShadowResourcePlanSummary
    {
        uint32 planned_texture_count = 0;
        uint32 planned_ssbo_count = 0;
        uint32 direct_texture_value_count = 0;
        uint32 missing_required_count = 0;
        uint32 missing_optional_count = 0;
        uint32 unused_recipe_texture_count = 0;
        uint32 unused_recipe_ssbo_count = 0;
    };

    const char *GetShadowResourcePlanErrorName(
        ShadowResourcePlanError error) noexcept;

    bool BuildShadowResourceAcquirePlan(
        const MaterialRecipe &recipe,
        const GLSLCodeModuleRegistry &registry,
        const ResolvedModuleGraph &module_graph,
        const ShaderInterfaceContract &shader_interface,
        const EffectiveMaterialProgramKey &effective_program,
        ResourceAcquirePlan &out_plan,
        ShadowResourcePlanSummary &out_summary,
        ShadowResourcePlanDiagnostic &out_diagnostic);

    bool CompareShadowResourcePlanToMaterialization(
        const ResourceAcquirePlan &plan,
        const MaterializationSpec &materialization,
        ShadowResourcePlanDiagnostic &out_diagnostic) noexcept;
}
