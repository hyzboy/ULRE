#pragma once

/// RecipeToKey.h — single canonical Recipe → MaterialKey conversion
///
/// Introduces ResolveRecipePrimaryKey() as the unique authoritative function
/// that converts a MaterialRecipe to a MaterialKey.  It is a pure function:
/// no global state, no I/O, no glslang dependency.
///
/// Usage (bakers and runtime alike):
///   MaterialKey key = hgl::graph::mtl::ResolveRecipePrimaryKey(recipe);
///
/// Note (Step-6 TODO): returns the *primary* key only.  When a Recipe expands
/// to multiple PassTypes via blend_mode, pass = GetPrimaryPassForBlendMode().
/// Full enumeration will be introduced in Step 6 (EnumerateRecipeKeys).

#include <hgl/mtl/MaterialKey.h>
#include <hgl/mtl/MaterialRecipe.h>

namespace hgl::graph::mtl
{
    /// Convert a MaterialRecipe to its canonical MaterialKey (primary pass only).
    /// Pure function — no side effects, no global state, no I/O, no glslang dependency.
    ///
    /// Fields left as placeholder zeros until Step 6:
    ///   def_id       — kInvalidStaticMaterialDefId (0)
    ///   glsl_version / vk_version / spv_version — 0
    MaterialKey ResolveRecipePrimaryKey(const MaterialRecipe &recipe) noexcept;

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
