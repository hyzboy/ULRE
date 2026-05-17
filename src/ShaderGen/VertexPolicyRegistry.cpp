#include <hgl/shadergen/VertexPolicyRegistry.h>

// src/ShaderGen/VertexPolicyRegistry.cpp
//
// Built-in vertex policy table.
//
// glsl_path entries are relative to the ShaderLibrary root so that the same
// path can be used for both the file-system include and the shader source
// #include directive.
//
// Policies tagged TerrainGrid / Sky / Text2D currently keep an empty path
// because those stages still use bespoke main_*.vert.glsl files (vs_template_path).
// They are listed here as reserved entries; once their bespoke VS files are
// migrated to the ApplyVertexTransform() contract the paths will be filled in.

namespace hgl::graph
{
    using Policy = mtl::VertexTransformPolicy;

    // Stable, ordered by Policy value.  Do NOT reorder or renumber entries.
    static const VertexPolicyDescriptor kBuiltinPolicies[] =
    {
        // policy                            glsl_path                                              needs_camera  needs_viewport
        { Policy::Unknown,              "",                                                         false,        false },
        { Policy::Mesh3D,               "ShaderLibrary/vertex_policy/mesh3d.glsl",                 true,         false },
        { Policy::Quad2D,               "ShaderLibrary/vertex_policy/quad2d.glsl",                 false,        false },
        { Policy::BillboardCameraFacing,"ShaderLibrary/vertex_policy/billboard_camera_facing.glsl",true,         false },
        { Policy::BillboardAxisLocked,  "ShaderLibrary/vertex_policy/billboard_axis_locked.glsl",  true,         true  },
        { Policy::TerrainGrid,          "",  /* TODO: migrate main_terrain_grid.vert.glsl */        true,         false },
        { Policy::Sky,                  "",  /* TODO: migrate sky VS */                             true,         false },
        { Policy::Text2D,               "",  /* TODO: migrate text2d.vert.glsl */                   true,         false },
        { Policy::FullscreenTriangle,   "ShaderLibrary/vertex_policy/passthrough_ndc.glsl",        false,        false },
    };

    static constexpr size_t kBuiltinPolicyCount =
        sizeof(kBuiltinPolicies) / sizeof(kBuiltinPolicies[0]);

    const VertexPolicyDescriptor *FindBuiltinVertexPolicy(mtl::VertexTransformPolicy policy) noexcept
    {
        for (size_t i = 0; i < kBuiltinPolicyCount; ++i)
            if (kBuiltinPolicies[i].policy == policy)
                return &kBuiltinPolicies[i];
        return nullptr;
    }

} // namespace hgl::graph
