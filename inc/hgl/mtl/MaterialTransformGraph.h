#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    struct MaterialTransformGraph
    {
        VertexInputMode source = VertexInputMode::Vec3Position;
        PositionMappingMode mapping = PositionMappingMode::Passthrough3D;
        OrientationMode orientation = OrientationMode::World;
        ScaleMode scale = ScaleMode::World;
        ProjectionMode projection = ProjectionMode::WorldCameraVP;

        VertexShaderNodeConfig ToNodeConfig() const noexcept
        {
            return VertexShaderNodeConfig{
                source, mapping, orientation, scale, projection
            };
        }

        static MaterialTransformGraph FromNodeConfig(
            const VertexShaderNodeConfig &config) noexcept
        {
            return MaterialTransformGraph{
                config.input,
                config.position_mapping,
                config.orientation,
                config.scale,
                config.projection
            };
        }

        static MaterialTransformGraph FlatXY() noexcept
        {
            return FromNodeConfig(Make2DNodeConfigNDC(true));
        }

        static MaterialTransformGraph PixelOrtho() noexcept
        {
            return FromNodeConfig(Make2DNodeConfigOrtho(false));
        }

        static MaterialTransformGraph WallXY() noexcept
        {
            MaterialTransformGraph graph = FlatXY();
            graph.mapping = PositionMappingMode::LiftXY_X0Y;
            return graph;
        }

        static MaterialTransformGraph World3D() noexcept
        {
            return FromNodeConfig(MakeDefault3DNodeConfig());
        }

        static MaterialTransformGraph Terrain() noexcept
        {
            MaterialTransformGraph graph = World3D();
            graph.mapping = PositionMappingMode::TerrainGrid;
            return graph;
        }

        uint64 GetHash() const noexcept
        {
            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(hash, source);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, mapping);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, orientation);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, scale);
            hash = hgl::hash::FNV1aAppendValueBytes(hash, projection);
            return hash;
        }

        bool IsScreenLike() const noexcept
        {
            if (source != VertexInputMode::Vec2Position
             && source != VertexInputMode::Vec2IntPosition)
                return false;

            switch (projection)
            {
            case ProjectionMode::LocalToWorldOnly:
            case ProjectionMode::OrthoViewport:
            case ProjectionMode::OrthoThenLocalToWorld:
            case ProjectionMode::ClipPassthrough:
                return true;
            case ProjectionMode::WorldCameraVP:
            default:
                return false;
            }
        }

        const char *GetMappingModulePath() const noexcept
        {
            switch (mapping)
            {
            case PositionMappingMode::LiftXY_XY0: return "vertex/s2_lift_xy0.glsl";
            case PositionMappingMode::LiftXY_X0Y: return "vertex/s2_lift_x0y.glsl";
            case PositionMappingMode::LiftXY_0XY: return "vertex/s2_lift_0xy.glsl";
            case PositionMappingMode::NDCLift: return "vertex/s2_ndc_lift.glsl";
            case PositionMappingMode::ZeroOneToNDC: return "vertex/s2_zeroone_to_ndc.glsl";
            case PositionMappingMode::PixelToLocal: return "vertex/s2_pixel_to_local.glsl";
            case PositionMappingMode::TerrainGrid: return nullptr;
            case PositionMappingMode::Passthrough3D:
            default: return "vertex/s2_passthrough3d.glsl";
            }
        }

        const char *GetStage3ModulePath() const noexcept
        {
            if (orientation == OrientationMode::CameraFacingFree
             || orientation == OrientationMode::CameraFacingAxisY)
            {
                return scale == ScaleMode::FixedPixelSize
                    ? "vertex/s3_camera_facing_fixed_pixels.glsl"
                    : "vertex/s3_camera_facing_world.glsl";
            }

            switch (projection)
            {
            case ProjectionMode::LocalToWorldOnly: return "vertex/s3_l2w_only.glsl";
            case ProjectionMode::OrthoViewport: return "vertex/s3_ortho_viewport.glsl";
            case ProjectionMode::OrthoThenLocalToWorld: return "vertex/s3_ortho_then_l2w.glsl";
            case ProjectionMode::ClipPassthrough: return "vertex/s3_clip_passthrough.glsl";
            case ProjectionMode::WorldCameraVP:
            default: return "vertex/s3_world_camera_vp.glsl";
            }
        }

        bool operator==(const MaterialTransformGraph &rhs) const noexcept
        {
            return source == rhs.source
                && mapping == rhs.mapping
                && orientation == rhs.orientation
                && scale == rhs.scale
                && projection == rhs.projection;
        }
    };
}
