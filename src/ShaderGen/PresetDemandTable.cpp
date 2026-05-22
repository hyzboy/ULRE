/// PresetDemandTable.cpp — Phase B
///
/// Flat, exhaustive table of PresetDemand entries for every MaterialPreset value.
/// Rules:
///   - required_va  : attribs the shader ALWAYS reads; supply MUST provide them (or derive_mask covers them).
///   - optional_va  : attribs used if available; missing ones → feature-stripped variant, not a fallback.
///   - derive_mask  : required attribs that can be computed in the VS when not directly supplied.
///   - fallback     : next preset to try; chain MUST terminate at Checkerboard3D.
///   - supports_dither_mask_transition : a DitherMask SPV variant exists for this preset.
///
/// Design: VA sets are expressed via VABits (bool[RANGE_SIZE]), never via hardcoded named bool fields.

#include <hgl/mtl/PresetDemandTable.h>
#include <hgl/common/VertexAttribDef.h>

namespace hgl::graph::mtl
{

// ---------------------------------------------------------------------------
// Build helpers — make table rows readable without repeating boilerplate
// ---------------------------------------------------------------------------
namespace
{

// Set one or more VA bits and return the result (chained).
static VABits VA(std::initializer_list<VertexAttrib> attribs) noexcept
{
    VABits b{};
    for (auto a : attribs)
        b.Set(a);
    return b;
}

// Shorthand aliases
static constexpr VertexAttrib kPos  = VertexAttrib::Position;
static constexpr VertexAttrib kNrm  = VertexAttrib::Normal;
static constexpr VertexAttrib kTan  = VertexAttrib::Tangent;
static constexpr VertexAttrib kBtan = VertexAttrib::Bitangent;
static constexpr VertexAttrib kCol  = VertexAttrib::Color;
static constexpr VertexAttrib kLum  = VertexAttrib::Luminance;
static constexpr VertexAttrib kUV   = VertexAttrib::TexCoord;
static constexpr VertexAttrib kJID  = VertexAttrib::JointID;

// Standard resource requirements
static const MaterialResourceRequirements kResNone{};

static const MaterialResourceRequirements kResMesh3D = [] {
    MaterialResourceRequirements r{};
    r.needs_transform           = true;
    r.needs_viewport            = true;
    r.needs_camera              = true;
    r.needs_material_instance   = true;
    return r;
}();

static const MaterialResourceRequirements kResLit3D = [] {
    MaterialResourceRequirements r = kResMesh3D;
    r.needs_sky                 = true;
    r.enable_lighting           = true;
    return r;
}();

static const MaterialResourceRequirements kRes2D = [] {
    MaterialResourceRequirements r{};
    r.needs_viewport            = true;
    r.needs_material_instance   = true;
    return r;
}();

static const MaterialResourceRequirements kResSky = [] {
    MaterialResourceRequirements r{};
    r.needs_viewport            = true;
    r.needs_camera              = true;
    r.needs_sky                 = true;
    r.needs_material_instance   = true;
    return r;
}();

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------
// clang-format off
static const PresetDemand kTable[] =
{
    // ── Error / Fallback ─────────────────────────────────────────────────────
    {
        /* preset     */ MaterialPreset::Checkerboard3D,
        /* required   */ VA({kPos}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,  // terminal
    },

    // ── Unified presets ───────────────────────────────────────────────────────
    {
        /* preset     */ MaterialPreset::VertexColor,
        /* required   */ VA({kPos, kCol}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::PureColor,
    },
    {
        /* preset     */ MaterialPreset::PureColor,
        /* required   */ VA({kPos}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },
    {
        /* preset     */ MaterialPreset::UnlitTexture,
        /* required   */ VA({kPos, kUV}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },
    {
        /* preset     */ MaterialPreset::VertexLuminance,
        /* required   */ VA({kPos, kLum}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::VertexColor,
    },
    {
        /* preset     */ MaterialPreset::Text2D,
        /* required   */ VA({kPos, kUV}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kRes2D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },

    // ── Specialised 3-D presets ───────────────────────────────────────────────
    {
        /* preset     */ MaterialPreset::VertexPaletteColor3D,
        /* required   */ VA({kPos, kCol}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::VertexColor,
    },
    {
        /* preset     */ MaterialPreset::Gizmo3D,
        /* required   */ VA({kPos, kNrm}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::PureColor,
    },
    {
        /* preset     */ MaterialPreset::TerrainGrid,
        /* required   */ VA({kPos}),
        /* optional   */ VA({kUV}),
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },
    {
        /* preset     */ MaterialPreset::SkyMinimal,
        /* required   */ VA({kPos}),
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResSky,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },

    // ── Standard (lit mesh; tangent/bitangent derivable from pos+uv+normal) ──
    {
        /* preset     */ MaterialPreset::Standard,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        // Tangent can be derived from Position+TexCoord in the VS.
        // Bitangent can then be computed as Normal × Tangent.
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::UnlitTexture,
    },
    {
        /* preset     */ MaterialPreset::PBRColor3D,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },

    // ── Semantic aliases (all route through Standard for now) ─────────────────
    // When a dedicated LOD/preset implementation is added, replace 'fallback'
    // and adjust required_va / optional_va accordingly.
    {
        /* preset     */ MaterialPreset::HumanSkin,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::AmphibiansSkin,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::Wood,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::TreeBark,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::Stone,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::Leaf,
        // Leaf / foliage: commonly Masked or DitherMask; tangent derivable.
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,   // DitherMask is the primary foliage transition technique
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::Metal,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::BirdFeathers,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },
    {
        /* preset     */ MaterialPreset::Scales,
        /* required   */ VA({kPos, kNrm, kUV}),
        /* optional   */ VA({kTan, kBtan}),
        /* derive     */ VA({kTan, kBtan}),
        /* resources  */ kResLit3D,
        /* dither     */ true,
        /* fallback   */ MaterialPreset::Standard,
    },

    // ── PCG / procedural ─────────────────────────────────────────────────────
    {
        /* preset     */ MaterialPreset::FullscreenTriangle,
        /* required   */ VABits{},  // procedural: no vertex input from geometry
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kRes2D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },

    // ── User-defined / fully custom ───────────────────────────────────────────
    {
        /* preset     */ MaterialPreset::Custom,
        /* required   */ VABits{},  // unknown until recipe provides vertex_provider_glsl
        /* optional   */ VABits{},
        /* derive     */ VABits{},
        /* resources  */ kResMesh3D,
        /* dither     */ false,
        /* fallback   */ MaterialPreset::Checkerboard3D,
    },
};
// clang-format on

static constexpr size_t kTableSize = sizeof(kTable) / sizeof(kTable[0]);

// Compile-time assertion: every preset from Checkerboard3D to FullscreenTriangle
// must have an entry. The comparison is done at runtime via the lookup below, but
// we at least assert the table covers the expected range.
static_assert(kTableSize == static_cast<size_t>(MaterialPreset::RANGE_SIZE),
    "PresetDemandTable: kTable entry count does not match MaterialPreset::RANGE_SIZE. "
    "Add a missing entry or remove a stale one.");

// Fallback entry returned when a preset is not found (should not happen at runtime).
static const PresetDemand kFallbackEntry = {
    MaterialPreset::Checkerboard3D,
    VA({kPos}), VABits{}, VABits{},
    kResMesh3D, false,
    MaterialPreset::Checkerboard3D,
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// GetPresetDemand
// ---------------------------------------------------------------------------
const PresetDemand& GetPresetDemand(MaterialPreset preset) noexcept
{
    for (size_t i = 0; i < kTableSize; ++i)
        if (kTable[i].preset == preset)
            return kTable[i];
    return kFallbackEntry;
}

// ---------------------------------------------------------------------------
// ResolveFallbackPreset
// ---------------------------------------------------------------------------
MaterialPreset ResolveFallbackPreset(
    MaterialPreset  start,
    const VABits&   supply) noexcept
{
    MaterialPreset current = start;
    constexpr int kMaxDepth = 16;   // guard against cycles

    for (int depth = 0; depth < kMaxDepth; ++depth)
    {
        const PresetDemand& demand = GetPresetDemand(current);
        if (SupplyMeetsDemand(demand, supply))
            return current;

        // Terminal condition: Checkerboard3D always succeeds (position only).
        if (demand.fallback == current)
            return MaterialPreset::Checkerboard3D;

        current = demand.fallback;
    }

    return MaterialPreset::Checkerboard3D;
}

// ---------------------------------------------------------------------------
// Phase C — ResolveEffectiveVABits
// ---------------------------------------------------------------------------
/// Computes the set of vertex attrib bits that should be set in the variant key
/// given the preset demand and the geometry supply.
///
/// Result = ((required ∪ optional) ∩ supply) ∪ (required ∩ derive_mask)
///
/// Explanation:
///   - Required attribs that are supplied → included.
///   - Optional attribs that are supplied → included (enables richer feature path).
///   - Required attribs not supplied but derivable → still included (VS derives them).
///   - Optional attribs not supplied            → excluded (feature-stripped variant).
///   - Required attribs not supplied and not derivable → excluded
///     (caller should have already fallen back via ResolveFallbackPreset).
VABits ResolveEffectiveVABits(const PresetDemand& demand, const VABits& supply) noexcept
{
    VABits result{};
    for (size_t i = 0; i < static_cast<size_t>(VertexAttrib::RANGE_SIZE); ++i)
    {
        const bool is_required = demand.required_va.bits[i];
        const bool is_optional = demand.optional_va.bits[i];
        const bool is_supplied = supply.bits[i];
        const bool is_derivable = demand.derive_mask.bits[i];

        if (is_supplied && (is_required || is_optional))
            result.bits[i] = true;
        else if (is_required && is_derivable)
            result.bits[i] = true;
    }
    return result;
}

} // namespace hgl::graph::mtl

