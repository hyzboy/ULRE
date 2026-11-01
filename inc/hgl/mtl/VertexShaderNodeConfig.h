#pragma once

#include <hgl/type/EnumUtil.h>
#include <hgl/serialize/EnumMeta.h>
#include <cstring>

namespace hgl::graph::mtl
{
    // Stage 1 — 顶点位置输入格式
    enum class VertexInputMode : uint8_t
    {
        Vec2Position = 0,  // layout(location=0) in vec2 Position
        Vec3Position,      // layout(location=0) in vec3 Position
        Vec2IntPosition,   // layout(location=0) in ivec2 Position (integer, e.g. text pixel coords)
        None,              // 无外部顶点输入（CharQuad 等自持全部 SSBO）

        ENUM_CLASS_RANGE(Vec2Position, None)
    };

    // 名字↔值的唯一真源 = 本文件末尾的 `HGL_ENUM_BEGIN` 登记块；
    // 取名字用 `hgl::serialize::EnumNameOf`，解析用 `hgl::serialize::EnumParse`。


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

        ENUM_CLASS_RANGE(Passthrough3D, PixelToLocal)
    };

    // 名字↔值的唯一真源 = 本文件末尾的 `HGL_ENUM_BEGIN` 登记块（见 `EnumNameOf` / `EnumParse`）。

    // Stage 2 映射模块**输入**使用的 Position 分量数。
    //
    // 与 position_*.glsl 顶部的 `// @ulre require GeometryAttribute Position Float N N`
    // 一一对应（Passthrough3D → 3，其余抬升类映射 → 2）。
    //
    // 为什么需要它：Stage 1 的 `stream_position_*` 由几何格式（VkFormat）决定，
    // Stage 2 的映射由材质 `position_mapping` 决定——两个所有者，彼此不可见。
    // 两边不一致时产出的是「能生成、编不过」的 GLSL
    // （例如 `vec4(vec3, 0.0, 1.0)` 五分量构造、`vec4(vec2, 1.0)` 三分量构造），
    // 所以必须在发射期显式对账。
    inline constexpr uint32_t GetPositionMappingComponentCount(
        const PositionMappingMode mode) noexcept
    {
        return mode == PositionMappingMode::Passthrough3D ? 3u : 2u;
    }


    // Stage 3a — 朝向策略
    enum class OrientationMode : uint8_t
    {
        World = 0,              // 应用 LocalToWorld 矩阵（标准 3D / 2D L2W）
        CameraFacingFree,       // Billboard：朝向相机，无轴约束
        CameraFacingAxisY,      // Billboard：朝向相机，Y 轴固定

        ENUM_CLASS_RANGE(World, CameraFacingAxisY)
    };

    // 名字↔值的唯一真源 = 本文件末尾的 `HGL_ENUM_BEGIN` 登记块（见 `EnumNameOf` / `EnumParse`）。


    // Stage 3b — 缩放策略
    enum class ScaleMode : uint8_t
    {
        World = 0,      // 世界空间缩放（普通 L2W 缩放）
        FixedPixelSize, // 固定像素大小（UI/HUD billboard）

        ENUM_CLASS_RANGE(World, FixedPixelSize)
    };

    // 名字↔值的唯一真源 = 本文件末尾的 `HGL_ENUM_BEGIN` 登记块（见 `EnumNameOf` / `EnumParse`）。


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

    // 名字↔值的唯一真源 = 本文件末尾的 `HGL_ENUM_BEGIN` 登记块（见 `EnumNameOf` / `EnumParse`）。


    // 顶点 Shader PCG 节点配置：三阶段五参数
    struct VertexShaderNodeConfig
    {
        VertexInputMode     input            = VertexInputMode::Vec3Position;
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
            PositionMappingMode::Passthrough3D,
            OrientationMode::World,
            ScaleMode::World,
            ProjectionMode::WorldCameraVP
        };
    }

    inline bool IsDefault3DNodeConfig(const VertexShaderNodeConfig &cfg) noexcept
    {
        return cfg.input == VertexInputMode::Vec3Position
            && cfg.position_mapping == PositionMappingMode::Passthrough3D
            && cfg.orientation == OrientationMode::World
            && cfg.scale == ScaleMode::World
            && cfg.projection == ProjectionMode::WorldCameraVP;
    }

    inline bool IsSameVertexNodeConfig(const VertexShaderNodeConfig &a,
                                       const VertexShaderNodeConfig &b) noexcept
    {
        return a.input == b.input
            && a.position_mapping == b.position_mapping
            && a.orientation == b.orientation
            && a.scale == b.scale
            && a.projection == b.projection;
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
// ── 枚举登记（拼写与 GUI 说明的唯一真源）─────────────────────────────────────

HGL_ENUM_BEGIN(hgl::graph::mtl::VertexInputMode)
    HGL_ENUM_ITEM(Vec2Position,   "layout(location=0) in vec2 Position")
    HGL_ENUM_ITEM(Vec3Position,   "layout(location=0) in vec3 Position")
    HGL_ENUM_ITEM(Vec2IntPosition,"layout(location=0) in ivec2 Position（整数，如文字像素坐标）")
    HGL_ENUM_ITEM(None,           "无外部顶点输入（CharQuad 等自持全部 SSBO）")
HGL_ENUM_END(hgl::graph::mtl::VertexInputMode)

HGL_ENUM_BEGIN(hgl::graph::mtl::PositionMappingMode)
    HGL_ENUM_ITEM(Passthrough3D, "vec4(Position,1.0)，标准 3D")
    HGL_ENUM_ITEM(LiftXY_XY0,    "2D→3D 抬升，Z=0")
    HGL_ENUM_ITEM(LiftXY_X0Y,    "2D→3D 抬升，贴地，Y 向上")
    HGL_ENUM_ITEM(LiftXY_0XY,    "2D→3D 抬升，X=0")
    HGL_ENUM_ITEM(NDCLift,       "输入已是 NDC XY，直接升维")
    HGL_ENUM_ITEM(ZeroOneToNDC,  "[0,1] 映射到 [-1,1] 后再升维")
    HGL_ENUM_ITEM(PixelToLocal,  "像素坐标直接作为本地坐标传递")
HGL_ENUM_END(hgl::graph::mtl::PositionMappingMode)

HGL_ENUM_BEGIN(hgl::graph::mtl::OrientationMode)
    HGL_ENUM_ITEM(World,              "应用 LocalToWorld 矩阵（标准 3D / 2D L2W）")
    HGL_ENUM_ITEM(CameraFacingFree,   "Billboard：朝向相机，无轴约束")
    HGL_ENUM_ITEM(CameraFacingAxisY,  "Billboard：朝向相机，Y 轴固定")
HGL_ENUM_END(hgl::graph::mtl::OrientationMode)

HGL_ENUM_BEGIN(hgl::graph::mtl::ScaleMode)
    HGL_ENUM_ITEM(World,          "世界空间缩放（普通 L2W 缩放）")
    HGL_ENUM_ITEM(FixedPixelSize, "固定像素大小（UI/HUD billboard）")
HGL_ENUM_END(hgl::graph::mtl::ScaleMode)

HGL_ENUM_BEGIN(hgl::graph::mtl::ProjectionMode)
    HGL_ENUM_ITEM(WorldCameraVP,          "camera.vp * world_pos（标准 3D 透视/正射）")
    HGL_ENUM_ITEM(LocalToWorldOnly,       "l2w * local_pos（无相机投影）")
    HGL_ENUM_ITEM(OrthoViewport,          "viewport.ortho_matrix * local_pos（2D Ortho）")
    HGL_ENUM_ITEM(OrthoThenLocalToWorld,  "l2w * (viewport.ortho_matrix * local_pos)")
    HGL_ENUM_ITEM(ClipPassthrough,        "已在 clip 空间，直接输出")
HGL_ENUM_END(hgl::graph::mtl::ProjectionMode)
