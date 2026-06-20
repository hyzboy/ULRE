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
#include <vector>

namespace hgl::graph::mtl
{
    struct Phase0RPolicyState
    {
        VertexTransformPolicy vertex_policy = VertexTransformPolicy::Unknown;
        VertexSourcePolicy vertex_source_policy = VertexSourcePolicy::Unknown;
        GeometryLiftPolicy geometry_lift_policy = GeometryLiftPolicy::Unknown;
        OrientationPolicy orientation_policy = OrientationPolicy::Unknown;
        SizePolicy size_policy = SizePolicy::Unknown;
    };

    /// Resolve the effective Phase0R policy tuple for a recipe without
    /// mutating routing behavior. Pure function, safe for unit tests.
    Phase0RPolicyState ResolvePhase0RPolicyState(const MaterialRecipe &recipe) noexcept;

    /// Validate the currently-resolved Phase0R tuple against local structural
    /// constraints. Returns nullptr when the tuple is locally valid.
    const char *ValidatePhase0RPolicyState(const Phase0RPolicyState &state) noexcept;

    /// Convert a MaterialRecipe to its canonical MaterialKey (primary pass only).
    /// Pure function — no side effects, no global state, no I/O, no glslang dependency.
    MaterialKey ResolveRecipePrimaryKey(const MaterialRecipe &recipe) noexcept;

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
