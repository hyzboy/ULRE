#pragma once

#include <hgl/mtl/MaterialProgramContract.h>

namespace hgl::graph::mtl
{
    struct MaterialRecipe;
    struct MaterialResourceLayout;

    enum class ActiveProfileBindingViewBuildError : uint8
    {
        None = 0,
        InvalidPreparedProgramSet,
        DuplicateRecipeTexture,
        DuplicateRecipeData,
        InvalidBindingView
    };

    struct ActiveProfileBindingViewBuildDiagnostic
    {
        ActiveProfileBindingViewBuildError error =
            ActiveProfileBindingViewBuildError::None;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        uint32 data_slot = 0;
        SSBOType ssbo_type = SSBOType::UserDefined;
    };

    const char *GetActiveProfileBindingViewBuildErrorName(
        ActiveProfileBindingViewBuildError error) noexcept;
    const char *GetActiveProfileBindingSourceName(
        ActiveProfileBindingSource source) noexcept;

    uint64 GetActiveProfileBindingSourceHash(
        const MaterialRecipe &recipe) noexcept;
    uint64 GetMaterialTextureAssetIdentityHash(
        const char *resource_id,
        uint32 resource_id_length) noexcept;
    uint64 GetMaterialDataAssetIdentityHash(
        SSBOType ssbo_type,
        uint32 ssbo_id,
        uint32 data_slot) noexcept;

    bool BuildActiveProfileBindingView(
        const MaterialRecipe &recipe,
        const MaterialResourceLayout &resource_layout,
        const PreparedMaterialProgramSet &prepared_set,
        ActiveProfileBindingView &out_view,
        ActiveProfileBindingViewBuildDiagnostic &out_diagnostic) noexcept;

    bool BuildActiveProfileMaterialRecipe(
        const MaterialRecipe &source_recipe,
        const ActiveProfileBindingView &binding_view,
        MaterialRecipe &out_recipe) noexcept;

    bool BuildActiveProfileResourceAcquirePlan(
        const ActiveProfileBindingView &binding_view,
        ResourceAcquirePlan &out_plan) noexcept;
}
