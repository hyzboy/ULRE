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
    struct DescriptorContract;
}

namespace hgl::graph::mtl
{
    using namespace hgl::graph::shadergen;

    bool BuildBindingTable(
        const MaterialRecipe &recipe,
        const DescriptorContract &layout,
        const shadergen::ShaderProgramKey &program_key,
        ResolvedBindingTable &out_table,
        BindingBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildBindingTable(
        const MaterialRecipe &recipe,
        const ShaderResourceSchema &layout,
        const shadergen::ShaderProgramKey &program_key,
        ResolvedBindingTable &out_table,
        BindingBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildBindingTableRecipe(
        const MaterialRecipe &source_recipe,
        const ResolvedBindingTable &binding_table,
        MaterialRecipe &out_recipe) noexcept;

    bool BuildResourceAcquirePlan(
        const ResolvedBindingTable &binding_table,
        ResourceAcquirePlan &out_plan) noexcept;
}
