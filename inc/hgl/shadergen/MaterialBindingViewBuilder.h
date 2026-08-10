#pragma once

#include <hgl/shadergen/ShaderProgramKey.h>
#include <hgl/mtl/MaterialBindingContract.h>

namespace hgl::graph::mtl
{
    struct MaterialRecipe;
    struct MaterialDescriptorContract;
    struct MaterialResourceLayout;

    bool BuildMaterialBindingView(
        const MaterialRecipe &recipe,
        const MaterialDescriptorContract &layout,
        const ShaderProgramKey &program_key,
        MaterialBindingView &out_view,
        MaterialBindingViewBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildMaterialBindingView(
        const MaterialRecipe &recipe,
        const MaterialResourceLayout &layout,
        const ShaderProgramKey &program_key,
        MaterialBindingView &out_view,
        MaterialBindingViewBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildMaterialBindingRecipe(
        const MaterialRecipe &source_recipe,
        const MaterialBindingView &binding_view,
        MaterialRecipe &out_recipe) noexcept;

    bool BuildMaterialResourceAcquirePlan(
        const MaterialBindingView &binding_view,
        ResourceAcquirePlan &out_plan) noexcept;
}
