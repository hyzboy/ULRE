#pragma once

#include <hgl/type/EnumUtil.h>
#include <string_view>

namespace hgl::graph
{
    /// Identifies which position-data source a vertex shader uses.
    ///
    /// Numeric values are stable and are embedded in the shader cache key.
    /// Built-in IDs 0–1023 must never be reordered or repurposed.
    /// User-defined IDs start at 0x8000 (lower 15 bits = hash of the glsl_path).
    enum class PositionProviderId : uint16
    {
        // ── Sentinel ─────────────────────────────────────────────────────────
        Unknown                = 0x7FFF, ///< Unset/invalid sentinel; must never reach shader compilation or registry lookup

        // ── Built-in IDs (stable) ────────────────────────────────────────────
        DirectVec3             = 0,  ///< VAB vec3 direct pass-through; emitter inlines a #define, zero overhead
        VAB_Vec2               = 1,  ///< VAB vec2; shader pads Z = 0
        PCG_FullscreenTriangle = 2,  ///< Procedural fullscreen triangle via gl_VertexIndex; no VAB; NDC space
        //SSBO_PackedVec3        = 3,  ///< gl_VertexIndex → vec3 read from storage buffer; no VAB
        //TerrainGrid            = 4,  ///< Grid (col, row) + heightmap sampler; no VAB
        //VAB_Packed16           = 5,  ///< 16-bit packed VAB (R16G16B16A16_SNORM decode); reserved

        // ── User-defined range ───────────────────────────────────────────────
        UserCustom_Begin       = 0x8000,
    };

    /// Returns true if the given provider is a procedural (PCG) source that
    /// generates vertex positions without consuming any VAB attributes.
    ///
    /// Single point of truth: when adding a new PCG_* provider above, add a
    /// matching case here.  Callers must NOT inline per-ID checks.
    constexpr bool IsPCGPositionProvider(PositionProviderId id) noexcept
    {
        switch (id)
        {
        case PositionProviderId::PCG_FullscreenTriangle:
            return true;
        default:
            return false;
        }
    }

    /// Describes one position provider: where the ID maps to, which GLSL file
    /// implements `GetPositionLocal()`, and what GPU resources it requires.
    ///
    /// The emitter consults this struct to:
    ///   - decide whether to inline a `#define` (DirectVec3) or emit an `#include`
    ///   - know how many vertex attribute slots to reserve
    ///   - merge resource requirements into the material manifest
    struct PositionProvider
    {
        PositionProviderId id;

        /// Path to the GLSL implementation file, relative to the shader library root.
        /// Empty for DirectVec3 — the emitter inlines the declaration directly.
        std::string_view   glsl_path;

        /// Number of vertex attribute bindings consumed (0 = no VAB, 1 = single VAB).
        uint8              vab_count    = 0;

        bool               needs_ssbo   = false;  ///< requires a storage buffer binding
        bool               needs_uniform = false;  ///< requires a UBO or push-constant block
        bool               needs_sampler = false;  ///< requires a texture sampler binding
    };

}//namespace hgl::graph
