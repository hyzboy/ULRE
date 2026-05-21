#pragma once

#include <hgl/type/EnumUtil.h>
#include <string_view>

namespace hgl::graph
{
    /// Identifies which position-data source a vertex shader uses.
    ///
    /// Numeric ranges (stable — never reorder or repurpose assigned IDs):
    ///   0x0000          Unknown   — zero-init sentinel, invalid
    ///   0x0001..0x00FF  VAB_*     — vertex-attribute-buffer stream shapes (placeholders; no .glsl required)
    ///   0x0100..0x0FFF  PCG_*     — built-in procedural generators
    ///   0x1000          UserPCG   — user-supplied .glsl; path hash stored separately in the variant key
    ///   0xFFFF          Invalid   — explicit error sentinel
    enum class PositionProviderId : uint16
    {
        // ── Sentinels ────────────────────────────────────────────────────────
        Unknown  = 0x0000, ///< Zero-init / unset; must never reach shader compilation
        Invalid  = 0xFFFF, ///< Explicit error value

        // ── VAB stream shapes (0x0001 – 0x00FF) ─────────────────────────────
        // Placeholders: an ID may exist without a corresponding .glsl today.
        // Callers must check that the registry entry has a non-empty glsl_path.
        VAB_Float          = 0x01, ///< layout(location=0) in float
        VAB_Vec2           = 0x02, ///< in vec2   — 2D ortho / UI
        VAB_Vec3           = 0x03, ///< in vec3   — standard 3D mesh  (was: DirectVec3)
        VAB_Vec4           = 0x04, ///< in vec4   — pre-multiplied clip-space w

        VAB_IFloat         = 0x05, ///< in int
        VAB_IVec2          = 0x06, ///< in ivec2  — Text2D pixel coord
        VAB_IVec3          = 0x07, ///< in ivec3  — voxel
        VAB_IVec4          = 0x08, ///< in ivec4  — reserved

        VAB_UFloat         = 0x09, ///< in uint   — reserved
        VAB_UVec2          = 0x0A, ///< in uvec2  — reserved
        VAB_UVec3          = 0x0B, ///< in uvec3  — reserved
        VAB_UVec4          = 0x0C, ///< in uvec4  — reserved

        VAB_BVec2          = 0x0D, ///< in bvec2  — reserved (rarely enters VS)
        VAB_BVec3          = 0x0E, ///< in bvec3  — reserved
        VAB_BVec4          = 0x0F, ///< in bvec4  — reserved

        VAB_DVec2          = 0x10, ///< in dvec2  — double-precision (CAD)
        VAB_DVec3          = 0x11, ///< in dvec3  — double-precision
        VAB_DVec4          = 0x12, ///< in dvec4  — double-precision

        VAB_Packed_RGB10A2 = 0x13, ///< uint → unpack vec3  (compact position)
        VAB_Packed_R16G16  = 0x14, ///< uint → unpack vec2  (compact 2-D)
        VAB_Packed_RGBA16F = 0x15, ///< uvec2 → unpack vec4 (half-precision)

        // ── Builtin PCG (0x0100 – 0x0FFF) ───────────────────────────────────
        PCG_FullscreenTriangle    = 0x0100, ///< gl_VertexIndex ∈ {0,1,2} → NDC full-screen triangle
        PCG_FullscreenQuad        = 0x0101, ///< 6-vertex full-screen quad
        PCG_UnitCube              = 0x0102, ///< 36-vertex unit cube
        PCG_UnitSphereIcosahedron = 0x0103, ///< procedural unit sphere (icosahedron subdivision)
        PCG_GridXZ                = 0x0104, ///< gl_VertexIndex → grid (x,z) vertex
        PCG_DebugAxes             = 0x0105, ///< 3 axis lines

        // ── User PCG (0x1000) ────────────────────────────────────────────────
        UserPCG = 0x1000, ///< User-supplied .glsl; path hash stored in MaterialVariantKey::user_provider_path_hash
    };

    // ── Range helpers (no switch — new IDs never require edits here) ─────────

    constexpr bool IsVABPositionProvider(PositionProviderId id) noexcept
    {
        const auto v = static_cast<uint16>(id);
        return v >= 0x0001u && v <= 0x00FFu;
    }

    constexpr bool IsBuiltinPCGPositionProvider(PositionProviderId id) noexcept
    {
        const auto v = static_cast<uint16>(id);
        return v >= 0x0100u && v <= 0x0FFFu;
    }

    constexpr bool IsUserPCGPositionProvider(PositionProviderId id) noexcept
    {
        return id == PositionProviderId::UserPCG;
    }

    constexpr bool IsPCGPositionProvider(PositionProviderId id) noexcept
    {
        return IsBuiltinPCGPositionProvider(id) || IsUserPCGPositionProvider(id);
    }

    /// True if the provider reads from a vertex attribute buffer.
    constexpr bool ConsumesVAB(PositionProviderId id) noexcept
    {
        return IsVABPositionProvider(id);
    }

    /// Describes one position provider: the .glsl path and GPU resource requirements.
    ///
    /// NOTE: This struct is the interim registry record used until the full
    /// ProviderManifest / @sfm system is in place (Phase 3).  Fields will be
    /// superseded by ProviderManifest and are kept for backward compatibility
    /// during the transition.
    struct PositionProvider
    {
        PositionProviderId id;

        /// Path to the GLSL implementation file, relative to the shader library root.
        /// Empty = placeholder ID with no .glsl yet; callers must guard against this.
        std::string_view   glsl_path;

        /// Number of vertex attribute bindings consumed (0 = no VAB, 1 = single VAB).
        uint8              vab_count     = 0;

        bool               needs_ssbo    = false; ///< requires a storage buffer binding
        bool               needs_uniform = false; ///< requires a UBO or push-constant block
        bool               needs_sampler = false; ///< requires a texture sampler binding
    };

}//namespace hgl::graph
