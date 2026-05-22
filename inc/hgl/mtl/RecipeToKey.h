#pragma once

/// RecipeToKey.h — single canonical Recipe → MaterialKey conversion
///
/// Introduces ResolveRecipePrimaryKey() as the unique authoritative function
/// that converts a MaterialRecipe to a MaterialKey.  It is a pure function:
/// no global state, no I/O, no glslang dependency.
///
/// Usage (bakers and runtime alike):
///   MaterialKey key = hgl::graph::mtl::ResolveRecipePrimaryKey(recipe);

#include <hgl/mtl/MaterialKey.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/PresetDemandTable.h>   // VABits (Phase C — to be removed with MaterialLibrary migration)
#include <vector>

namespace hgl::graph::mtl
{
    /// Convert a MaterialRecipe to its canonical MaterialKey (primary pass only).
    /// Pure function — no side effects, no global state, no I/O, no glslang dependency.
    MaterialKey ResolveRecipePrimaryKey(const MaterialRecipe &recipe) noexcept;

    /// Phase C variant: same as ResolveRecipePrimaryKey() but also accepts the
    /// geometry vertex-attrib supply as a VABits descriptor.
    ///
    /// The supply is used to:
    ///   1. Run demand∩supply matching via ResolveFallbackPreset() — if the recipe's
    ///      preset cannot be satisfied by the supplied attribs, a simpler fallback
    ///      preset is selected automatically.
    ///   2. Derive the exact vertex_attribute_feature_bits via ResolveEffectiveVABits()
    ///      instead of the demand table-driven approach.
    ///
    /// When supply is empty (all-false VABits{}) the function falls back to the
    /// legacy path — identical to calling ResolveRecipePrimaryKey().
    MaterialKey ResolveRecipePrimaryKeyWithSupply(
        const MaterialRecipe &recipe,
        const VABits         &geometry_supply) noexcept;

    /// Expand a recipe to all MaterialKeys it requires (one per pass type).
    /// The pass field of each key differs; all other fields are identical.
    /// The vector is ordered by the pass tables in PassExpansion.cpp.
    std::vector<MaterialKey> EnumerateRecipeKeys(const MaterialRecipe &recipe);

    /// Unit-test helpers that expose the intermediate canonicalize steps.
    namespace detail
    {
        /// Build an un-canonicalized MaterialVariantKey from a recipe by merging
        /// the preset base key with all recipe-specific field overrides.
        MaterialVariantKey BuildBaseVariantKeyFromRecipe(const MaterialRecipe &recipe) noexcept;

        /// Apply per-preset router canonicalization rules (e.g. Standard + Mesh3D
        /// forces sky_ambient_model = Simple).
        MaterialVariantKey ApplyRouterCanonicalization(const MaterialVariantKey &key) noexcept;

        /// Return the primary PassType for a given blend mode.
        /// Mirrors the first entry of CompositorAssembler::GetPassTypesForBlendMode.
        PassType GetPrimaryPassForBlendMode(RenderAlphaMode blend) noexcept;
    } // namespace detail

} // namespace hgl::graph::mtl
