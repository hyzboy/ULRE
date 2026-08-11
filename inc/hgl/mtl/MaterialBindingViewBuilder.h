#pragma once

#include <hgl/shadergen/ShaderProgramKey.h>
#include <hgl/mtl/MaterialBindingContract.h>

namespace hgl::graph::mtl
{
    struct MaterialRecipe;
    struct ShaderResourceSchema;
}
namespace hgl::graph::shadergen
{
    struct MaterialDescriptorContract;
}

namespace hgl::graph::mtl
{
    using namespace hgl::graph::shadergen;

    bool BuildMaterialBindingView(
        const MaterialRecipe &recipe,
        const MaterialDescriptorContract &layout,
        const shadergen::ShaderProgramKey &program_key,
        MaterialBindingView &out_view,
        MaterialBindingViewBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildMaterialBindingView(
        const MaterialRecipe &recipe,
        const ShaderResourceSchema &layout,
        const shadergen::ShaderProgramKey &program_key,
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
