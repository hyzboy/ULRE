#include <hgl/shadergen/PositionProviderRegistry.h>

namespace hgl::graph
{
    using Id = PositionProviderId;

    // Stable, ordered by ID value.  Do NOT reorder or renumber entries.
    static const PositionProvider kBuiltinProviders[] =
    {
        //  id                             glsl_path                                                              vab  ssbo   ubo    samp
        { Id::DirectVec3,             "ShaderLibrary/position_provider/vab_vec3.glsl",                            1, false, false, false },
        { Id::VAB_Vec2,               "ShaderLibrary/position_provider/vab_vec2.glsl",                            1, false, false, false },
        { Id::PCG_FullscreenTriangle, "ShaderLibrary/position_provider/pcg_fullscreen_triangle.glsl",             0, false, false, false },
//        { Id::SSBO_PackedVec3,        "ShaderLibrary/position_provider/ssbo_packed.glsl",                         0, true,  false, false },
//        { Id::TerrainGrid,            "ShaderLibrary/position_provider/terrain_grid.glsl",                        0, false, true,  true  },
    };

    static constexpr size_t kBuiltinProviderCount =
        sizeof(kBuiltinProviders) / sizeof(kBuiltinProviders[0]);

    const PositionProvider *FindBuiltinProvider(PositionProviderId id) noexcept
    {
        for (size_t i = 0; i < kBuiltinProviderCount; ++i)
            if (kBuiltinProviders[i].id == id)
                return &kBuiltinProviders[i];
        return nullptr;
    }

}//namespace hgl::graph
