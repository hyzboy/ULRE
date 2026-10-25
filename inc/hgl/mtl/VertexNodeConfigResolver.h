#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/util/hash/FNV1a.h>
#include <hgl/log/Log.h>

namespace hgl::graph::mtl
{
    /// Pure-static utility for resolving GLSL vertex stage module paths
    /// and querying transform properties from a VertexShaderNodeConfig.
    struct VertexNodeConfigResolver
    {
        /// 位置映射的分量数是否与 Stage 1 顶点输入模块一致。
        ///
        /// Stage 1 的 `s1_position_*` 由几何 VkFormat 决定，Stage 2 的映射由材质
        /// `position_mapping` 决定——两个所有者，彼此不可见。分量数不一致时生成器会
        /// 「成功」产出一份编不过的 GLSL（构造参数个数不匹配），错误还会指向 s2 模块，
        /// 极易误判。因此在发射期硬门，口径与 s2_*.glsl 的
        /// `@ulre require GeometryAttribute Position Float N N` 同源。
        ///
        /// context：出错时指认来源（definition_id / mesh mode name）。
        static bool CheckPositionComponentCount(const VertexShaderNodeConfig &cfg,
                                                const uint32_t s1_component_count,
                                                const char *context) noexcept
        {
            const uint32_t required = GetPositionMappingComponentCount(cfg.position_mapping);
            if (required == s1_component_count)
                return true;

            GLogError(u8"位置映射与顶点位置输入不匹配：position_mapping=%s 需要 Position 分量数 %u，"
                      u8"而 Stage 1 输入模块提供 %u 个分量（%s）",
                      GetPositionMappingModeName(cfg.position_mapping),
                      required, s1_component_count,
                      context ? context : "");
            return false;
        }

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
            case PositionMappingMode::Passthrough3D:    return "vertex/s2_passthrough3d.glsl";
            default:                                    return nullptr;   // 未映射的枚举值 = 映射缺失，调用方必须硬失败而非静默兜底
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

        /// s3 模块是否提供 `GetWorldPos` / `GetWorldNormal`。
        ///
        /// 只有真正建立了世界空间的模块才提供这对接口：
        ///  - `s3_world_camera_vp`       L2W → camera.vp
        ///  - `s3_l2w_only`              纯 L2W（2D NDC/ZeroOne）
        ///  - `s3_ortho_then_l2w`        ortho → L2W
        ///  - `s3_camera_facing_world`   相机朝向、世界单位尺寸
        /// 其余三种**不提供**：
        ///  - `s3_camera_facing_fixed_pixels` 位置由 clip 空间像素偏移直接给出
        ///  - `s3_ortho_viewport`             只有 viewport.ortho_matrix，无 L2W
        ///  - `s3_clip_passthrough`           原样透传，语义上没有世界空间
        /// 材质若在这些模式上要求 WorldPosition/WorldNormal 语义，
        /// 生成期必须硬失败（硬编码兜底会静默产出错值）。
        static bool HasWorldSpace(const VertexShaderNodeConfig &cfg) noexcept
        {
            if (cfg.orientation == OrientationMode::CameraFacingFree
             || cfg.orientation == OrientationMode::CameraFacingAxisY)
            {
                return cfg.scale != ScaleMode::FixedPixelSize;
            }

            switch (cfg.projection)
            {
            case ProjectionMode::LocalToWorldOnly:
            case ProjectionMode::OrthoThenLocalToWorld:
            case ProjectionMode::WorldCameraVP:
                return true;
            case ProjectionMode::OrthoViewport:
            case ProjectionMode::ClipPassthrough:
            default:
                return false;
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
    };
}
