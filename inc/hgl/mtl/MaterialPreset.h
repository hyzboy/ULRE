#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph::mtl
{
    // MaterialLOD is a runtime material implementation level, not a synonym for user-facing
    // graphics quality settings. It may be selected from multiple signals, including platform
    // capability, user quality preferences, object distance, screen-space importance, tile-level
    // lighting complexity, or even per-pixel classification in a deferred/VBuffer pipeline.
    //
    // In the planned VBuffer path, an earlier pass writes MaterialID and MaterialBindingInstanceID into
    // the VBuffer, and a later compute stage classifies screen-space tiles and shading workload.
    // Pixels or regions that are sufficiently far away, visually unimportant, or under constrained
    // shading budget may be downgraded to a lower material implementation level automatically.
    // Therefore MaterialLOD is not required to be object-wide, model-wide, or quality-preset-wide.
    enum class MaterialLOD : uint8
    {
        Base,

        ENUM_CLASS_RANGE(Base, Base)
    };

    // MaterialPreset expresses semantic material intent, not a fixed shader recipe list.
    // A preset such as HumanSkin / BirdFeathers / TreeBark stands for an authoring-facing
    // material solution family. The actual shader implementation is expected to vary by
    // runtime material LOD and platform/rendering-path capability.
    //
    // Examples:
    // - BirdFeathers may resolve to a higher-end dual-lobe GGX path on PC.
    // - The same BirdFeathers preset may resolve to a simpler Blinn-Phong style path on mobile.
    // - In a VBuffer deferred path, distant or low-importance pixels may resolve to a cheaper
    //   BirdFeathers implementation than nearby pixels of the same object.
    // - Features such as ParallaxMap are implementation details selected by runtime LOD, not by preset.
    //
    // The current stage only provides a single built-in LOD level (MaterialLOD::Base), so newly
    // added semantic presets temporarily route to Standard to keep all existing programs running.
    // This is a bootstrap state rather than the final architecture.
    enum class MaterialPreset : uint8
    {
        // Error/Fallback material
        Checkerboard3D,    ///< Gray checkerboard pattern for missing/error cases

        VertexColor2D,
        PureColor2D,
        PureTexture2D,
        Text2D,

        PureColor3D,
        VertexColor3D,
        VertexLuminance3D,
        VertexPattleColor3D,
        Gizmo3D,
        TerrainGrid,
        SkyMinimal,
        Billboard2DDynamic,
        Billboard2DFixed,
        Standard,
        PBRColor3D,
        VertexLuminance2D,

        // Semantic presets (currently aliases to Standard pipeline)
        HumanSkin,
        AmphibiansSkin,
        Wood,
        TreeBark,
        Stone,
        Leaf,
        Metal,
        BirdFeathers,
        Scales,

        ENUM_CLASS_RANGE(Checkerboard3D,Scales)
    };
}
