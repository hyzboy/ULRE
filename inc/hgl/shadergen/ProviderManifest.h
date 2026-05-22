#pragma once

#include <hgl/common/PositionProvider.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/mtl/RenderPhase.h>
#include <string>
#include <string_view>
#include <vector>
#include <bitset>
#include <cstdint>

namespace hgl::graph
{
    // ── OutputSpace ──────────────────────────────────────────────────────────
    // Describes the coordinate space in which vec4 GetPosition() delivers its
    // result.  The compositor uses this to decide whether an MVP transform is
    // needed before writing gl_Position.
    enum class OutputSpace : uint8_t
    {
        Local   = 0, ///< object/local space (w=1); compositor applies MVP
        World   = 1, ///< world space (w=1); compositor applies VP only
        ClipNDC = 2, ///< already in clip/NDC; compositor emits as-is, no MVP
    };

    // ── ProviderKind ─────────────────────────────────────────────────────────
    enum class ProviderKind : uint8_t
    {
        Unknown = 0,
        VAB     = 1, ///< reads from vertex attribute buffer
        PCG     = 2, ///< procedural generation (no VAB consumed)
    };

    // ── InputSource ──────────────────────────────────────────────────────────
    enum class InputSource : uint8_t
    {
        None  = 0,
        VAB   = 1,
        SSBO  = 2,
        UBO   = 3,
        Push  = 4,
    };

    // ── ProviderManifest ─────────────────────────────────────────────────────
    /// Runtime record for one position provider, populated from @sfm headers.
    ///
    /// All providers implement the unified output contract:
    ///   vec4 GetPosition()
    /// The meaning of the returned vec4 is given by output_space.
    struct ProviderManifest
    {
        // ── Identity ─────────────────────────────────────────────────────────
        PositionProviderId  pos_id          = PositionProviderId::Unknown;
        std::string         glsl_path;              ///< ShaderLibrary-relative; empty = placeholder
        uint32_t            glsl_path_hash  = 0;    ///< FNV-1a 32 of glsl_path (UserPCG key)
        ProviderKind        kind            = ProviderKind::Unknown;
        uint16_t            sfm_version     = 0;    ///< @sfm version field; must be 1

        // ── @sfm metadata ────────────────────────────────────────────────────
        bool        consumes_vab        = false; ///< reads ≥1 VAB attribute
        bool        needs_ssbo          = false;
        bool        needs_ubo           = false;
        bool        needs_sampler       = false;
        bool        allow_dim_override  = false; ///< VAB=true; PCG=false
        OutputSpace output_space        = OutputSpace::Local;

        // ── Vertex attribute requirements (NEW: SFM-driven) ──────────────────
        using VABitset = std::bitset<static_cast<size_t>(VertexAttrib::RANGE_SIZE)>;
        VABitset    va_required;    ///< Must be present
        VABitset    va_optional;    ///< May be used if available
        VABitset    va_derive;      ///< Can be derived from required/optional

        // ── Resource requirements (NEW: SFM-driven) ───────────────────────────
        std::vector<std::string> tex_required;   ///< Required texture sampler names
        std::vector<std::string> ubo_required;   ///< Required UBO names
        std::vector<std::string> ssbo_required;  ///< Required SSBO names

        // ── Sky & phase support (NEW: SFM-driven) ─────────────────────────────
        bool        needs_sky = false;           ///< Requires sky/ambient
        using PhaseBitset = std::bitset<static_cast<size_t>(mtl::RenderPhase::COUNT)>;
        PhaseBitset supports_phase;              ///< Which render phases this shader supports

        // ── Input stream descriptors ──────────────────────────────────────────
        struct InputSpec
        {
            InputSource source      = InputSource::None;
            std::string attrib_name;
            std::string format;     ///< "vec3", "vec2", "RGB10A2", ...
        };
        std::vector<InputSpec> inputs;
    };

    // ── ProviderManifestRegistry ─────────────────────────────────────────────
    /// Global registry of all position provider manifests.
    ///
    /// Lifecycle:
    ///   1. Call Initialize(lib_root) once at ShaderGen startup to scan
    ///      ShaderLibrary/position_provider/ and build manifests.
    ///   2. Call RunSelfCheck() immediately after Initialize() to validate all
    ///      manifests (Debug: asserts; Release: logs errors).
    ///   3. Use Find*() / AcquireUserProvider() at runtime; never call
    ///      Initialize() again after startup.
    class ProviderManifestRegistry
    {
    public:
        /// Scan <lib_root>/position_provider/*.glsl, parse @sfm headers,
        /// and populate the registry.  Safe to call multiple times (re-initializes).
        static void Initialize(std::string_view lib_root);

        /// Clear all registered manifests. Call at shutdown, paired with Initialize.
        static void Shutdown();

        // ── Lookup ───────────────────────────────────────────────────────────

        /// Find a builtin manifest by PositionProviderId.
        /// Returns nullptr for Unknown, Invalid, UserPCG, or unregistered IDs.
        static const ProviderManifest* FindByPosId(PositionProviderId id) noexcept;

        /// Find a manifest by its ShaderLibrary-relative path.
        static const ProviderManifest* FindByGlslPath(std::string_view path) noexcept;

        /// Find a UserPCG or builtin manifest by FNV-1a 32 hash of its glsl_path.
        static const ProviderManifest* FindByPathHash(uint32_t hash) noexcept;

        // ── UserPCG ──────────────────────────────────────────────────────────

        /// Register (or return existing) manifest for a user-supplied .glsl path.
        /// Parses the @sfm header on first call; pos_id is set to UserPCG.
        /// Returns nullptr on parse failure (warning logged; caller should fallback).
        static const ProviderManifest* AcquireUserProvider(std::string_view glsl_path);

        // ── Diagnostics ──────────────────────────────────────────────────────

        /// Validate all registered manifests:
        ///   - sfm_version == 1
        ///   - glsl_path is non-empty and the file exists
        ///   - output_space is a legal value
        ///   - consumes_vab ↔ inputs not empty (for VAB providers)
        ///   - no hash collisions between entries
        ///
        /// Debug builds: HGL_ASSERT on first failure.
        /// Release builds: logs each error, returns false if any fail.
        static bool RunSelfCheck();

        /// Return total number of registered manifests (for diagnostics).
        static size_t Count() noexcept;
    };

    // ── FNV-1a 32 helper (used by the registry; exposed for callers) ─────────
    constexpr uint32_t Fnv1a32(std::string_view s) noexcept
    {
        uint32_t h = 2166136261u;
        for (unsigned char c : s)
            h = (h ^ c) * 16777619u;
        return h;
    }

    // ── SFM annotation parsing helpers ───────────────────────────────────────
    /// Parse a vertex attribute name -> VertexAttrib enum.
    /// Returns VertexAttrib::RANGE_SIZE if not recognized.
    VertexAttrib VertexAttribFromName(std::string_view name) noexcept;

    /// Parse a render phase name -> RenderPhase enum.
    /// Returns RenderPhase::COUNT if not recognized.
    mtl::RenderPhase RenderPhaseFromName(std::string_view name) noexcept;

} // namespace hgl::graph
