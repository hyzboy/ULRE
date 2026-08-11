#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    /// Pure-static utility for resolving GLSL vertex stage module paths
    /// and querying transform properties from a VertexShaderNodeConfig.
    struct VertexNodeConfigResolver
    {
        static const char *GetMappingModulePath(const VertexShaderNodeConfig &cfg) noexcept
        {
            switch (cfg.position_mapping)
            {
            case PositionMappingMode::LiftXY_XY0:      return "vertex/s2_lift_xy0.glsl";
            case PositionMappingMode::LiftXY_X0Y:      return "vertex/s2_lift_x0y.glsl";
            case PositionMappingMode::LiftXY_0XY:      return "vertex/s2_lift_0xy.glsl";
            case PositionMappingMode::NDCLift:          return "vertex/s2_ndc_lift.glsl";
            case PositionMappingMode::ZeroOneToNDC:     return "vertex/s2_zeroone_to_ndc.glsl";
            case PositionMappingMode::PixelToLocal:     return "vertex/s2_pixel_to_local.glsl";
            case PositionMappingMode::TerrainGrid:      return nullptr;
            case PositionMappingMode::Passthrough3D:
            default:                                   return "vertex/s2_passthrough3d.glsl";
            }
        }

        static const char *GetStage3ModulePath(const VertexShaderNodeConfig &cfg) noexcept
        {
            if (cfg.orientation == OrientationMode::CameraFacingFree
             || cfg.orientation == OrientationMode::CameraFacingAxisY)
            {
                return cfg.scale == ScaleMode::FixedPixelSize
                    ? "vertex/s3_camera_facing_fixed_pixels.glsl"
                    : "vertex/s3_camera_facing_world.glsl";
            }

            switch (cfg.projection)
            {
            case ProjectionMode::LocalToWorldOnly:      return "vertex/s3_l2w_only.glsl";
            case ProjectionMode::OrthoViewport:          return "vertex/s3_ortho_viewport.glsl";
            case ProjectionMode::OrthoThenLocalToWorld:  return "vertex/s3_ortho_then_l2w.glsl";
            case ProjectionMode::ClipPassthrough:        return "vertex/s3_clip_passthrough.glsl";
            case ProjectionMode::WorldCameraVP:
            default:                                    return "vertex/s3_world_camera_vp.glsl";
            }
        }

        static bool IsScreenLike(const VertexShaderNodeConfig &cfg) noexcept
        {
            if (cfg.input != VertexInputMode::Vec2Position
             && cfg.input != VertexInputMode::Vec2IntPosition)
                return false;

            switch (cfg.projection)
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

        static uint64 GetHash(const VertexShaderNodeConfig &cfg) noexcept
        {
            hgl::hash::FNV1aHasher64 h;

            h << cfg.input
              << cfg.position_mapping
              << cfg.orientation
              << cfg.scale
              << cfg.projection;

            return h;
        }

        // ── Factory methods ──────────────────────────────────────────

        static VertexShaderNodeConfig FlatXY() noexcept
        {
            return Make2DNodeConfigNDC(true);
        }

        static VertexShaderNodeConfig PixelOrtho() noexcept
        {
            return Make2DNodeConfigOrtho(false);
        }

        static VertexShaderNodeConfig WallXY() noexcept
        {
            VertexShaderNodeConfig cfg = Make2DNodeConfigNDC(true);
            cfg.position_mapping = PositionMappingMode::LiftXY_X0Y;
            return cfg;
        }

        static VertexShaderNodeConfig World3D() noexcept
        {
            return MakeDefault3DNodeConfig();
        }

        static VertexShaderNodeConfig Terrain() noexcept
        {
            VertexShaderNodeConfig cfg = MakeDefault3DNodeConfig();
            cfg.position_mapping = PositionMappingMode::TerrainGrid;
            return cfg;
        }
    };
}
