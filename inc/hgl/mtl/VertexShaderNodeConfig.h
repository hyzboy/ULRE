#pragma once

#include <hgl/type/EnumUtil.h>
namespace hgl::graph::mtl
{
    // Stage 1 — 顶点位置输入格式
    enum class VertexInputMode : uint8_t
    {
        Vec2Position = 0,  // layout(location=0) in vec2 Position
        Vec3Position,      // layout(location=0) in vec3 Position
        Vec2IntPosition,   // layout(location=0) in ivec2 Position (integer, e.g. text pixel coords)
        Procedural,        // 无 VBO 位置输入（依赖 gl_VertexID 等）

        ENUM_CLASS_RANGE(Vec2Position, Procedural)
    };

    // 顶点数据传输方式（MeshShader 方向：顶点输入统一为 SSBO）
    enum class VertexTransportMode : uint8_t
    {
        VBO = 0,    // 传统 vertex attribute（默认）
        SSBO,       // 顶点数据 SSBO（gl_VertexIndex 读取，s1_* 模块）

        ENUM_CLASS_RANGE(VBO, SSBO)
    };

    // Stage 2 — 物体空间位置映射策略
    // 输入为 Stage 1 的原始顶点位置，输出 vec4 local_pos（物体空间）
    enum class PositionMappingMode : uint8_t
    {
        Passthrough3D = 0,  // vec4(Position, 1.0)  — 标准 3D
        LiftXY_XY0,         // vec4(Position, 0.0, 1.0)  — 2D→3D，Z=0
        LiftXY_X0Y,         // vec4(Position.x, 0.0, Position.y, 1.0)  — 贴地，Y向上
        LiftXY_0XY,         // vec4(0.0, Position, 1.0)
        NDCLift,            // 输入已是 NDC XY，直接升维（同 LiftXY_XY0 语义）
        ZeroOneToNDC,       // [0,1] → [-1,1]，再升维
        PixelToLocal,       // 像素坐标输入，直接传递（Ortho 投影前的本地坐标）
        TerrainGrid,        // 程序化地形，依赖 gl_VertexID

        ENUM_CLASS_RANGE(Passthrough3D, TerrainGrid)
    };

    // Stage 3a — 朝向策略
    enum class OrientationMode : uint8_t
    {
        World = 0,              // 应用 LocalToWorld 矩阵（标准 3D / 2D L2W）
        CameraFacingFree,       // Billboard：朝向相机，无轴约束
        CameraFacingAxisY,      // Billboard：朝向相机，Y 轴固定

        ENUM_CLASS_RANGE(World, CameraFacingAxisY)
    };

    // Stage 3b — 缩放策略
    enum class ScaleMode : uint8_t
    {
        World = 0,      // 世界空间缩放（普通 L2W 缩放）
        FixedPixelSize, // 固定像素大小（UI/HUD billboard）

        ENUM_CLASS_RANGE(World, FixedPixelSize)
    };

    // Stage 3c — 投影策略
    enum class ProjectionMode : uint8_t
    {
        WorldCameraVP = 0,  // camera.vp * world_pos（标准 3D 透视/正射）
        LocalToWorldOnly,   // l2w * local_pos（旧 2D NDC/ZeroToOne + HAS_L2W）
        OrthoViewport,      // viewport.ortho_matrix * local_pos（2D Ortho）
        OrthoThenLocalToWorld, // l2w * (viewport.ortho_matrix * local_pos)（旧 2D Ortho + HAS_L2W）
        ClipPassthrough,    // 已在 clip 空间，直接输出（NDC 输入材质）

        ENUM_CLASS_RANGE(WorldCameraVP, ClipPassthrough)
    };

    // 顶点 Shader PCG 节点配置：三阶段五参数
    struct VertexShaderNodeConfig
    {
        VertexInputMode     input            = VertexInputMode::Vec3Position;
        VertexTransportMode transport        = VertexTransportMode::VBO;
        PositionMappingMode position_mapping = PositionMappingMode::Passthrough3D;
        OrientationMode     orientation      = OrientationMode::World;
        ScaleMode           scale            = ScaleMode::World;
        ProjectionMode      projection       = ProjectionMode::WorldCameraVP;
    };

    // 构造默认 3D 节点配置
    inline VertexShaderNodeConfig MakeDefault3DNodeConfig() noexcept
    {
        return VertexShaderNodeConfig{
            VertexInputMode::Vec3Position,
            VertexTransportMode::VBO,
            PositionMappingMode::Passthrough3D,
            OrientationMode::World,
            ScaleMode::World,
            ProjectionMode::WorldCameraVP
        };
    }

    inline bool IsDefault3DNodeConfig(const VertexShaderNodeConfig &cfg) noexcept
    {
        return cfg.input == VertexInputMode::Vec3Position
            && cfg.transport == VertexTransportMode::VBO
            && cfg.position_mapping == PositionMappingMode::Passthrough3D
            && cfg.orientation == OrientationMode::World
            && cfg.scale == ScaleMode::World
            && cfg.projection == ProjectionMode::WorldCameraVP;
    }

    inline VertexShaderNodeConfig Make2DNodeConfigNDC(bool local_to_world) noexcept
    {
        VertexShaderNodeConfig cfg;
        cfg.input = VertexInputMode::Vec2Position;
        cfg.position_mapping = PositionMappingMode::NDCLift;
        cfg.orientation      = OrientationMode::World;
        cfg.scale            = ScaleMode::World;
        cfg.projection       = local_to_world ? ProjectionMode::LocalToWorldOnly
                                              : ProjectionMode::ClipPassthrough;
        return cfg;
    }

    inline VertexShaderNodeConfig Make2DNodeConfigZeroToOne(bool local_to_world) noexcept
    {
        VertexShaderNodeConfig cfg;
        cfg.input = VertexInputMode::Vec2Position;
        cfg.position_mapping = PositionMappingMode::ZeroOneToNDC;
        cfg.orientation      = OrientationMode::World;
        cfg.scale            = ScaleMode::World;
        cfg.projection       = local_to_world ? ProjectionMode::LocalToWorldOnly
                                              : ProjectionMode::ClipPassthrough;
        return cfg;
    }

    inline VertexShaderNodeConfig Make2DNodeConfigOrtho(bool local_to_world) noexcept
    {
        VertexShaderNodeConfig cfg;
        cfg.input = VertexInputMode::Vec2Position;
        cfg.position_mapping = PositionMappingMode::PixelToLocal;
        cfg.orientation      = OrientationMode::World;
        cfg.scale            = ScaleMode::World;
        cfg.projection       = local_to_world ? ProjectionMode::OrthoThenLocalToWorld
                                              : ProjectionMode::OrthoViewport;
        return cfg;
    }

} // namespace hgl::graph::mtl
