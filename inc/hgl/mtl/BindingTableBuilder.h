#pragma once

#include <hgl/mtl/ShaderProgramKey.h>
#include <hgl/mtl/MaterialBindingContract.h>

namespace hgl::graph::mtl
{
    struct MaterialRecipe;
    struct ShaderResourceSchema;
}
namespace hgl::graph::mtl
{
    
    bool BuildBindingTable(
        const MaterialRecipe &recipe,
        const ShaderResourceSchema &layout,
        const mtl::ShaderProgramKey &program_key,
        ResolvedBindingTable &out_table,
        BindingBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildBindingTableRecipe(
        const MaterialRecipe &source_recipe,
        const ResolvedBindingTable &binding_table,
        MaterialRecipe &out_recipe) noexcept;
}
