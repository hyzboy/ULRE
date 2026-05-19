#include <hgl/shadergen/VertexPolicyRegistry.h>
#include <hgl/shadergen/ShaderRequirementSet.h>
#include <mutex>

// src/ShaderGen/VertexPolicyRegistry.cpp
//
// Built-in vertex policy table.
//
// glsl_path entries are relative to the ShaderLibrary root so that the same
// path can be used for both the file-system include and the shader source
// #include directive.
//
// needs_camera / needs_viewport / needs_transform are populated lazily on first
// access by parsing @sfm:require annotations from the policy GLSL file.  They
// must NOT be hand-coded here; the shader is the single source of truth.

namespace hgl::graph
{
    using Policy = mtl::VertexTransformPolicy;

    // Mutable table – needs_* fields are filled in lazily.
    static VertexPolicyDescriptor kBuiltinPolicies[] =
    {
        // policy                              glsl_path (relative to ShaderLibrary root)
        { Policy::Unknown,               "" },
        { Policy::Mesh3D,                "vertex_policy/mesh3d.glsl" },
        { Policy::Quad2D,                "vertex_policy/quad2d.glsl" },
        { Policy::BillboardCameraFacing, "vertex_policy/billboard_camera_facing.glsl" },
        { Policy::BillboardAxisLocked,   "vertex_policy/billboard_axis_locked.glsl" },
        { Policy::TerrainGrid,           "" }, // TODO: migrate main_terrain_grid.vert.glsl
        { Policy::Sky,                   "vertex_policy/sky.glsl" },
        { Policy::Text2D,                "" }, // TODO: migrate text2d.vert.glsl
        { Policy::FullscreenTriangle,    "vertex_policy/passthrough_ndc.glsl" },
        { Policy::Position2DTransform,   "vertex_policy/position2d_transform.glsl" },
        { Policy::Position2DNdc,         "vertex_policy/position2d_ndc.glsl" },
        { Policy::Position2DZeroToOne,   "vertex_policy/position2d_zero_to_one.glsl" },
        { Policy::Position2DOrtho,       "vertex_policy/position2d_ortho.glsl" },
    };

    static constexpr size_t kBuiltinPolicyCount =
        sizeof(kBuiltinPolicies) / sizeof(kBuiltinPolicies[0]);

    // Each entry has its own once_flag so parsing happens exactly once per policy.
    static std::once_flag kParseFlags[kBuiltinPolicyCount];

    static void ParsePolicyRequirements(VertexPolicyDescriptor &desc)
    {
        if (desc.glsl_path.empty())
            return;

        hgl::graph::ShaderRequirementSet req;
        req.ParseFromGLSLFile(desc.glsl_path);

        desc.needs_camera    = req.Requires("camera");
        desc.needs_viewport  = req.Requires("viewport");
        desc.needs_transform = req.Requires("transform_id") || req.Requires("transform_data");
    }

    const VertexPolicyDescriptor *FindBuiltinVertexPolicy(mtl::VertexTransformPolicy policy) noexcept
    {
        for (size_t i = 0; i < kBuiltinPolicyCount; ++i)
        {
            if (kBuiltinPolicies[i].policy == policy)
            {
                std::call_once(kParseFlags[i], ParsePolicyRequirements,
                               std::ref(kBuiltinPolicies[i]));
                return &kBuiltinPolicies[i];
            }
        }
        return nullptr;
    }
} // namespace hgl::graph
