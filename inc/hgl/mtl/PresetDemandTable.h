#pragma once
/// PresetDemandTable.h — Phase B
///
/// For every MaterialPreset, declares:
///   - required_va   : vertex attribs the shader ALWAYS needs from geometry supply
///   - optional_va   : vertex attribs the shader CAN use if supply provides them
///   - derive_mask   : attribs that CAN be derived/computed if supply is missing
///                     (e.g. tangent derived from position+uv; bitangent from n×t)
///   - resources     : which UBO/SSBO/Sky slots the preset requires
///   - supports_dither_mask_transition : whether a DitherMask SPV variant exists
///   - fallback      : next preset to try if demand ∩ supply is unsatisfied
///
/// Design rules (copilot-instructions):
///   - VA sets are expressed as bool arrays indexed by VertexAttrib (RANGE_SIZE),
///     not as hardcoded named fields, so future attrib additions require no
///     structural changes here.
///   - The table is flat and exhaustive; there are no hidden branches.
///   - GetPresetDemand() is the single authoritative accessor; callers must not
///     hard-code per-preset logic.

#include <hgl/common/VertexAttribDef.h>
#include <hgl/mtl/MaterialPreset.h>
#include <hgl/mtl/MaterialVariantRow.h>   // MaterialResourceRequirements

namespace hgl::graph::mtl
{

// ---------------------------------------------------------------------------
// VABits — bool array keyed by VertexAttrib, size = VertexAttrib::RANGE_SIZE
// ---------------------------------------------------------------------------
struct VABits
{
    bool bits[static_cast<size_t>(VertexAttrib::RANGE_SIZE)] = {};

    constexpr bool Get(VertexAttrib a) const noexcept
    {
        return bits[static_cast<size_t>(a)];
    }
    constexpr void Set(VertexAttrib a, bool v = true) noexcept
    {
        bits[static_cast<size_t>(a)] = v;
    }

    /// Convert to the legacy uint32 bitmask used by BuiltinVariantEntry / MaterialVariantKey.
    uint32 ToFeatureBits() const noexcept
    {
        uint32 result = 0;
        for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
            if (bits[i])
                result |= VertexAttribFeatureBit(static_cast<VertexAttrib>(i));
        return result;
    }

    /// Populate from a uint32 bitmask (bridge from legacy paths).
    void FromFeatureBits(uint32 mask) noexcept
    {
        for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
            bits[i] = (mask & VertexAttribFeatureBit(static_cast<VertexAttrib>(i))) != 0;
    }

    /// Returns true if every bit set in 'req' is also set in this instance (supply check).
    bool Satisfies(const VABits& req) const noexcept
    {
        for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
            if (req.bits[i] && !bits[i])
                return false;
        return true;
    }

    VABits operator|(const VABits& o) const noexcept
    {
        VABits out;
        for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
            out.bits[i] = bits[i] || o.bits[i];
        return out;
    }

    bool operator==(const VABits& o) const noexcept
    {
        for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
            if (bits[i] != o.bits[i])
                return false;
        return true;
    }
    bool operator!=(const VABits& o) const noexcept { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// PresetDemand — demand declaration for one MaterialPreset
// ---------------------------------------------------------------------------
struct PresetDemand
{
    MaterialPreset preset = MaterialPreset::Checkerboard3D;

    /// Vertex attribs the shader ALWAYS reads; geometry MUST supply these.
    VABits required_va{};

    /// Vertex attribs the shader CAN use if geometry supplies them.
    /// Missing optional attribs cause a variant with fewer features, not a fallback.
    VABits optional_va{};

    /// Attribs that are listed as required but MAY be derived at runtime if the
    /// geometry does NOT supply them directly.
    ///   e.g. Tangent can be derived from Position+TexCoord in the VS.
    ///        Bitangent can be computed as Normal × Tangent.
    /// When derive_mask has a bit set:
    ///   - If geometry supplies the attrib → use it directly (better quality).
    ///   - If geometry does NOT supply it   → derive it in the shader (acceptable).
    ///   Both paths are valid; the bake set must contain a variant for each case.
    VABits derive_mask{};

    /// UBO/SSBO/Sky resource requirements for this preset.
    MaterialResourceRequirements resources{};

    /// Whether a DitherMask SPV variant exists for this preset.
    /// Enables the TransitionState → blend=Dither upgrade path in
    /// ComposeEffectiveRenderState().
    bool supports_dither_mask_transition = false;

    /// Fallback preset when demand ∩ supply is still not satisfied after
    /// applying derive_mask. Must eventually terminate at Checkerboard3D.
    MaterialPreset fallback = MaterialPreset::Checkerboard3D;
};

// ---------------------------------------------------------------------------
// GetPresetDemand — single accessor; never returns nullptr
// ---------------------------------------------------------------------------
/// Returns a reference to the immutable PresetDemand entry for the given preset.
/// If the preset is not registered (future addition), returns the Checkerboard3D
/// error-indicator entry so callers never receive an invalid reference.
const PresetDemand& GetPresetDemand(MaterialPreset preset) noexcept;

// ---------------------------------------------------------------------------
// Convenience helpers
// ---------------------------------------------------------------------------

/// Returns true if the geometry supply (as VABits) satisfies the preset's
/// required_va, accounting for derive_mask (derived attribs are treated as
/// available even when not directly supplied).
inline bool SupplyMeetsDemand(
    const PresetDemand& demand,
    const VABits&       supply) noexcept
{
    for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
    {
        if (!demand.required_va.bits[i])
            continue;   // not required — skip
        if (supply.bits[i])
            continue;   // supplied directly — ok
        if (demand.derive_mask.bits[i])
            continue;   // can be derived — ok
        return false;   // required, not supplied, not derivable
    }
    return true;
}

/// Walk the fallback chain until a preset whose demand is satisfied is found.
/// Returns Checkerboard3D if nothing works (terminal error-indicator preset).
MaterialPreset ResolveFallbackPreset(
    MaterialPreset  start,
    const VABits&   supply) noexcept;

} // namespace hgl::graph::mtl
