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

    bool BuildBindingTable(
        const MaterialRecipe &recipe,
        const MaterialDescriptorContract &layout,
        const shadergen::ShaderProgramKey &program_key,
        ResolvedBindingTable &out_view,
        BindingBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildBindingTable(
        const MaterialRecipe &recipe,
        const ShaderResourceSchema &layout,
        const shadergen::ShaderProgramKey &program_key,
        ResolvedBindingTable &out_view,
        BindingBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildBindingTableRecipe(
        const MaterialRecipe &source_recipe,
        const ResolvedBindingTable &binding_view,
        MaterialRecipe &out_recipe) noexcept;

    bool BuildResourceAcquirePlan(
        const ResolvedBindingTable &binding_view,
        ResourceAcquirePlan &out_plan) noexcept;
}
