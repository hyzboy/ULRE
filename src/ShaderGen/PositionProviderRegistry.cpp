#include <hgl/shadergen/PositionProviderRegistry.h>

namespace hgl::graph
{
    using Id = PositionProviderId;

    // Full provider table — one entry per defined PositionProviderId.
    // Entries are sorted by ID value (ascending).
    //
    // glsl_path rules:
    //   - Non-empty  : a real .glsl exists and can be compiled.
    //   - Empty ("")  : placeholder ID; no .glsl yet.
    //                   Callers MUST check for empty path before use.
    //                   (CompositorAssembler will route-time-fallback to VAB_Vec3.)
    //
    // NOTE: This table is the interim registry until the full ProviderManifest /
    // @sfm system lands in Phase 3.  At that point each non-empty entry will gain
    // a corresponding ProviderManifest populated from @sfm headers.
    static const PositionProvider kBuiltinProviders[] =
    {
        //  id                                glsl_path                                                                   vab  ssbo   ubo    samp
        // ── VAB scalar / float ────────────────────────────────────────────────────────────────────────────────────────
        { Id::VAB_Float,          "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_Vec2,           "ShaderLibrary/position_provider/vab_vec2.glsl",                                        1, false, false, false },
        { Id::VAB_Vec3,           "ShaderLibrary/position_provider/vab_vec3.glsl",                                        1, false, false, false },
        { Id::VAB_Vec4,           "",                                                                                      1, false, false, false }, // placeholder
        // ── VAB integer ──────────────────────────────────────────────────────────────────────────────────────────────
        { Id::VAB_IFloat,         "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_IVec2,          "ShaderLibrary/position_provider/vab_ivec2.glsl",                                       1, false, false, false }, // UI / Text2D pixel coord
        { Id::VAB_IVec3,          "",                                                                                      1, false, false, false }, // placeholder (voxel)
        { Id::VAB_IVec4,          "",                                                                                      1, false, false, false }, // placeholder
        // ── VAB unsigned ─────────────────────────────────────────────────────────────────────────────────────────────
        { Id::VAB_UFloat,         "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_UVec2,          "ShaderLibrary/position_provider/vab_uvec2.glsl",                                       1, false, false, false }, // UI / 2D game sprite coord
        { Id::VAB_UVec3,          "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_UVec4,          "",                                                                                      1, false, false, false }, // placeholder
        // ── VAB bool (rarely enter VS directly) ──────────────────────────────────────────────────────────────────────
        { Id::VAB_BVec2,          "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_BVec3,          "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_BVec4,          "",                                                                                      1, false, false, false }, // placeholder
        // ── VAB double ───────────────────────────────────────────────────────────────────────────────────────────────
        { Id::VAB_DVec2,          "",                                                                                      1, false, false, false }, // placeholder (CAD)
        { Id::VAB_DVec3,          "",                                                                                      1, false, false, false }, // placeholder
        { Id::VAB_DVec4,          "",                                                                                      1, false, false, false }, // placeholder
        // ── VAB packed formats ────────────────────────────────────────────────────────────────────────────────────────
        { Id::VAB_Packed_RGB10A2, "",                                                                                      1, false, false, false }, // placeholder (compact vec3)
        { Id::VAB_Packed_R16G16,  "",                                                                                      1, false, false, false }, // placeholder (compact vec2)
        { Id::VAB_Packed_RGBA16F, "",                                                                                      1, false, false, false }, // placeholder (half-precision vec4)
        // ── Builtin PCG ──────────────────────────────────────────────────────────────────────────────────────────────
        { Id::PCG_FullscreenTriangle,    "ShaderLibrary/position_provider/pcg_fullscreen_triangle.glsl",                  0, false, false, false },
        { Id::PCG_FullscreenQuad,        "",                                                                               0, false, false, false }, // placeholder
        { Id::PCG_UnitCube,              "",                                                                               0, false, false, false }, // placeholder
        { Id::PCG_UnitSphereIcosahedron, "",                                                                               0, false, false, false }, // placeholder
        { Id::PCG_GridXZ,                "",                                                                               0, false, false, false }, // placeholder
        { Id::PCG_DebugAxes,             "",                                                                               0, false, false, false }, // placeholder
    };

    static constexpr size_t kBuiltinProviderCount =
        sizeof(kBuiltinProviders) / sizeof(kBuiltinProviders[0]);

    // Pre-built ID list (parallel to kBuiltinProviders)
    static const PositionProviderId kBuiltinProviderIds[] =
    {
        Id::VAB_Float, Id::VAB_Vec2, Id::VAB_Vec3, Id::VAB_Vec4,
        Id::VAB_IFloat, Id::VAB_IVec2, Id::VAB_IVec3, Id::VAB_IVec4,
        Id::VAB_UFloat, Id::VAB_UVec2, Id::VAB_UVec3, Id::VAB_UVec4,
        Id::VAB_BVec2, Id::VAB_BVec3, Id::VAB_BVec4,
        Id::VAB_DVec2, Id::VAB_DVec3, Id::VAB_DVec4,
        Id::VAB_Packed_RGB10A2, Id::VAB_Packed_R16G16, Id::VAB_Packed_RGBA16F,
        Id::PCG_FullscreenTriangle, Id::PCG_FullscreenQuad,
        Id::PCG_UnitCube, Id::PCG_UnitSphereIcosahedron,
        Id::PCG_GridXZ, Id::PCG_DebugAxes,
    };
    static_assert(sizeof(kBuiltinProviderIds)/sizeof(kBuiltinProviderIds[0]) == kBuiltinProviderCount,
                  "kBuiltinProviderIds out of sync with kBuiltinProviders");

    const PositionProvider *FindBuiltinProvider(PositionProviderId id) noexcept
    {
        // UserPCG is never in this table; the caller resolves it via path hash.
        if (id == PositionProviderId::Unknown  ||
            id == PositionProviderId::Invalid  ||
            id == PositionProviderId::UserPCG)
            return nullptr;

        for (size_t i = 0; i < kBuiltinProviderCount; ++i)
            if (kBuiltinProviders[i].id == id)
                return &kBuiltinProviders[i];
        return nullptr;
    }

    const PositionProviderId *GetAllBuiltinProviderIds(size_t *out_count) noexcept
    {
        if (out_count) *out_count = kBuiltinProviderCount;
        return kBuiltinProviderIds;
    }

}//namespace hgl::graph
