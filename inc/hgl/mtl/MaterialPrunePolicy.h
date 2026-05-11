#pragma once
/// MaterialPrunePolicy.h — Phase 4: Pass/quality driven resource pruning table.
///
/// This file defines the pruning model described in
/// SKY_RESOURCE_REQUIREMENT_REFACTOR_PLAN.md §4.3 / Phase 4:
///
///   MaterialResourceRequirements (authored, from builtin row)
///       ↓ apply PassPrunePolicy
///       ↓ apply QualityPrunePolicy
///       → effective MaterialResourceRequirements
///
/// Pruning rules are table-driven and are NOT scattered across material factories.
/// A pruning entry says "for this pass/quality combination, DISALLOW certain resources".
/// The absence of a matching entry means all resources are allowed (no pruning).

#include <hgl/mtl/MaterialVariantRow.h>
#include <hgl/mtl/PassType.h>

namespace hgl::graph::mtl
{
    using hgl::graph::PassType;
    /// Quality level for material variant selection / resource pruning.
    /// Future JSON material tables should use this as an explicit per-row axis.
    enum class MaterialQuality : uint8
    {
        High   = 0, ///< Full-quality forward shading, keeps all declared resources.
        Medium = 1, ///< May drop expensive optional resources (e.g. sky ambient).
        Low    = 2, ///< Drops sky, fancy lighting; minimal resource set.

        ENUM_CLASS_RANGE(High, Low)
    };

    /// Pruning flags for one (pass, quality) combination.
    /// Each flag, when set to false, DISALLOWS the corresponding resource from the
    /// effective requirements even if the builtin row declares it.
    struct MaterialVariantPrunePolicy
    {
        PassType        pass    = PassType::ForwardOpaque;
        MaterialQuality quality = MaterialQuality::High;

        bool allow_sky                      = true;
        bool allow_lighting                 = true;
        bool allow_material_instance        = true;
        bool allow_material_texture_index   = true;
        bool allow_camera                   = true;
        bool allow_viewport                 = true;
        bool allow_transform                = true;
        bool allow_color_palette            = true;
    };

    /// Apply a single prune policy to an authored resource requirements struct.
    /// Returns a new struct with disallowed resources cleared.
    inline MaterialResourceRequirements ApplyPrunePolicy(
        const MaterialResourceRequirements &authored,
        const MaterialVariantPrunePolicy   &policy) noexcept
    {
        MaterialResourceRequirements eff = authored;
        if (!policy.allow_sky)                    { eff.needs_sky = false; }
        if (!policy.allow_lighting)               { eff.enable_lighting = false; }
        if (!policy.allow_material_instance)      { eff.needs_material_instance = false; }
        if (!policy.allow_material_texture_index) { eff.needs_material_texture_index = false; }
        if (!policy.allow_camera)                 { eff.needs_camera = false; }
        if (!policy.allow_viewport)               { eff.needs_viewport = false; }
        if (!policy.allow_transform)              { eff.needs_transform = false; }
        if (!policy.allow_color_palette)          { eff.needs_color_palette = false; }
        return eff;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Built-in pruning table
    // ─────────────────────────────────────────────────────────────────────────

    /// The default (identity) policy — allows everything; used for ForwardOpaque/High.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyFullForward = {
        PassType::ForwardOpaque, MaterialQuality::High,
        /*allow_sky*/                    true,
        /*allow_lighting*/               true,
        /*allow_material_instance*/      true,
        /*allow_material_texture_index*/ true,
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          true,
    };

    /// Shadow opaque: prune sky, material texture index (no LOD texture selectors needed),
    /// keep transform and a minimal camera for depth calculation.
    /// lighting is pruned — shadow maps write depth, not lit colour.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyShadowOpaque = {
        PassType::ShadowOpaque, MaterialQuality::High,
        /*allow_sky*/                    false,
        /*allow_lighting*/               false,
        /*allow_material_instance*/      true,   // alpha-test data may still live here
        /*allow_material_texture_index*/ false,
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          false,
    };

    /// Shadow masked: same as ShadowOpaque but alpha-test material instance must be kept.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyShadowMasked = {
        PassType::ShadowMasked, MaterialQuality::High,
        /*allow_sky*/                    false,
        /*allow_lighting*/               false,
        /*allow_material_instance*/      true,
        /*allow_material_texture_index*/ true,   // alpha mask texture selector needed
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          false,
    };

    /// EarlyZ solid: depth prepass — sky, lighting, MI all pruned.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyEarlyZSolid = {
        PassType::EarlyZSolid, MaterialQuality::High,
        /*allow_sky*/                    false,
        /*allow_lighting*/               false,
        /*allow_material_instance*/      false,
        /*allow_material_texture_index*/ false,
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          false,
    };

    /// EarlyZ masked: depth prepass with alpha clip — needs MI for alpha test.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyEarlyZMasked = {
        PassType::EarlyZMasked, MaterialQuality::High,
        /*allow_sky*/                    false,
        /*allow_lighting*/               false,
        /*allow_material_instance*/      true,
        /*allow_material_texture_index*/ true,
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          false,
    };

    /// Forward opaque at medium quality: same as High except sky is pruned.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyForwardMedium = {
        PassType::ForwardOpaque, MaterialQuality::Medium,
        /*allow_sky*/                    false,
        /*allow_lighting*/               true,
        /*allow_material_instance*/      true,
        /*allow_material_texture_index*/ true,
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          true,
    };

    /// Forward opaque at low quality: sky + color palette pruned.
    inline constexpr MaterialVariantPrunePolicy kPrunePolicyForwardLow = {
        PassType::ForwardOpaque, MaterialQuality::Low,
        /*allow_sky*/                    false,
        /*allow_lighting*/               true,
        /*allow_material_instance*/      true,
        /*allow_material_texture_index*/ true,
        /*allow_camera*/                 true,
        /*allow_viewport*/               true,
        /*allow_transform*/              true,
        /*allow_color_palette*/          false,
    };

    // ─────────────────────────────────────────────────────────────────────────
    // Table lookup
    // ─────────────────────────────────────────────────────────────────────────

    /// Look up the prune policy for a given (pass, quality) pair.
    /// Returns a pointer into the static table, or nullptr if no entry matches
    /// (caller should treat nullptr as "full forward" / no pruning).
    inline const MaterialVariantPrunePolicy *
    FindPrunePolicy(const PassType pass, const MaterialQuality quality) noexcept
    {
        static const MaterialVariantPrunePolicy kTable[] = {
            kPrunePolicyFullForward,
            kPrunePolicyShadowOpaque,
            kPrunePolicyShadowMasked,
            kPrunePolicyEarlyZSolid,
            kPrunePolicyEarlyZMasked,
            kPrunePolicyForwardMedium,
            kPrunePolicyForwardLow,
        };

        for (const auto &entry : kTable)
        {
            if (entry.pass == pass && entry.quality == quality)
                return &entry;
        }

        return nullptr;
    }

    /// Derive the effective resource requirements for a given (authored row, pass, quality).
    /// If no prune policy is found for the combination, the authored requirements are returned unchanged.
    inline MaterialResourceRequirements DeriveEffectiveResources(
        const MaterialResourceRequirements &authored,
        const PassType                      pass,
        const MaterialQuality               quality) noexcept
    {
        const MaterialVariantPrunePolicy *policy = FindPrunePolicy(pass, quality);
        if (!policy)
            return authored;

        return ApplyPrunePolicy(authored, *policy);
    }

    /// Convenience overload that reads pass and quality from a MaterialVariantKey.
    /// Quality defaults to High when not otherwise available from the key.
    inline MaterialResourceRequirements DeriveEffectiveResources(
        const MaterialResourceRequirements &authored,
        const PassType                      pass) noexcept
    {
        return DeriveEffectiveResources(authored, pass, MaterialQuality::High);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Phase 5: manifest ↔ effective-policy cross-check helpers
    //
    // These operate on the semantic level (UBODescriptorSemantic) rather than
    // on the full MaterialResourceManifest type to avoid pulling in the heavy
    // MaterialResourceManifest / StaticMaterialDef headers here.
    //
    // Callers who have a full MaterialResourceManifest should check individual
    // UBO sets; helpers below cover the most common sky-policy scenario.
    // ─────────────────────────────────────────────────────────────────────────

    /// Returns true if the effective policy allows the SkyInfo UBO to be present.
    inline bool PolicyAllowsSky(const MaterialResourceRequirements &effective) noexcept
    {
        return effective.needs_sky;
    }

    /// Describes the result of a manifest-vs-policy validation for one UBO semantic.
    enum class PolicyManifestCheckResult : uint8
    {
        OK,                    ///< Manifest and policy agree.
        PolicyForbids,         ///< Manifest contains a resource that policy disallows (violation).
        PolicyRequires,        ///< Policy requires a resource that manifest omits (pruned by reflection).
    };

    /// Check whether a specific UBO (identified by whether it is "present in manifest")
    /// agrees with the policy's allow flag.
    inline PolicyManifestCheckResult CheckUBOAgainstPolicy(
        const bool manifest_has_ubo,
        const bool policy_allows_ubo) noexcept
    {
        if (manifest_has_ubo && !policy_allows_ubo)
            return PolicyManifestCheckResult::PolicyForbids;
        if (!manifest_has_ubo && policy_allows_ubo)
            return PolicyManifestCheckResult::PolicyRequires;
        return PolicyManifestCheckResult::OK;
    }

} // namespace hgl::graph::mtl
