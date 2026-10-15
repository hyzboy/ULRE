#pragma once

#include <hgl/type/EnumUtil.h>
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

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    //   GetVertexInputModeName：枚举 → 名字（switch）
    //   ParseVertexInputMode：名字 → 枚举（线性查表）
    // 新增模式只需改枚举 + 本 switch（名字表随之自动覆盖）。
    // 2026-10-14（S3b-3）：原为 `MaterialDefinitionFile.cpp` 的本地单向表（名字只在那里活着）。
    inline constexpr uint32_t VertexInputModeCount =
        static_cast<uint32_t>(VertexInputMode::None) + 1;

    inline const char *GetVertexInputModeName(const VertexInputMode mode) noexcept
    {
        switch (mode)
        {
        case VertexInputMode::Vec2Position: return "Vec2Position";
        case VertexInputMode::Vec3Position: return "Vec3Position";
        case VertexInputMode::Vec2IntPosition: return "Vec2IntPosition";
        case VertexInputMode::None:           return "None";
        }
        return "Vec3Position";
    }

    inline bool ParseVertexInputMode(const char *name, VertexInputMode &out) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < VertexInputModeCount; ++i)
        {
            const VertexInputMode mode = static_cast<VertexInputMode>(i);
            if (std::strcmp(name, GetVertexInputModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }


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

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    //   GetPositionMappingModeName：枚举 → 名字（switch）
    //   ParsePositionMappingMode：名字 → 枚举（线性查表）
    // 新增模式只需改枚举 + 本 switch（名字表随之自动覆盖）。
    // 2026-10-14（S3b-3）：原为 `MaterialDefinitionFile.cpp` 的本地单向表（名字只在那里活着）。
    inline constexpr uint32_t PositionMappingModeCount =
        static_cast<uint32_t>(PositionMappingMode::PixelToLocal) + 1;

    inline const char *GetPositionMappingModeName(const PositionMappingMode mode) noexcept
    {
        switch (mode)
        {
        case PositionMappingMode::Passthrough3D: return "Passthrough3D";
        case PositionMappingMode::LiftXY_XY0: return "LiftXY_XY0";
        case PositionMappingMode::LiftXY_X0Y: return "LiftXY_X0Y";
        case PositionMappingMode::LiftXY_0XY: return "LiftXY_0XY";
        case PositionMappingMode::NDCLift: return "NDCLift";
        case PositionMappingMode::ZeroOneToNDC: return "ZeroOneToNDC";
        case PositionMappingMode::PixelToLocal: return "PixelToLocal";
        }
        return "Passthrough3D";
    }

    inline bool ParsePositionMappingMode(const char *name, PositionMappingMode &out) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < PositionMappingModeCount; ++i)
        {
            const PositionMappingMode mode = static_cast<PositionMappingMode>(i);
            if (std::strcmp(name, GetPositionMappingModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }


    // Stage 3a — 朝向策略
    enum class OrientationMode : uint8_t
    {
        World = 0,              // 应用 LocalToWorld 矩阵（标准 3D / 2D L2W）
        CameraFacingFree,       // Billboard：朝向相机，无轴约束
        CameraFacingAxisY,      // Billboard：朝向相机，Y 轴固定

        ENUM_CLASS_RANGE(World, CameraFacingAxisY)
    };

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    //   GetOrientationModeName：枚举 → 名字（switch）
    //   ParseOrientationMode：名字 → 枚举（线性查表）
    // 新增模式只需改枚举 + 本 switch（名字表随之自动覆盖）。
    // 2026-10-14（S3b-3）：原为 `MaterialDefinitionFile.cpp` 的本地单向表（名字只在那里活着）。
    inline constexpr uint32_t OrientationModeCount =
        static_cast<uint32_t>(OrientationMode::CameraFacingAxisY) + 1;

    inline const char *GetOrientationModeName(const OrientationMode mode) noexcept
    {
        switch (mode)
        {
        case OrientationMode::World:         return "World";
        case OrientationMode::CameraFacingFree: return "CameraFacingFree";
        case OrientationMode::CameraFacingAxisY: return "CameraFacingAxisY";
        }
        return "World";
    }

    inline bool ParseOrientationMode(const char *name, OrientationMode &out) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < OrientationModeCount; ++i)
        {
            const OrientationMode mode = static_cast<OrientationMode>(i);
            if (std::strcmp(name, GetOrientationModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }


    // Stage 3b — 缩放策略
    enum class ScaleMode : uint8_t
    {
        World = 0,      // 世界空间缩放（普通 L2W 缩放）
        FixedPixelSize, // 固定像素大小（UI/HUD billboard）

        ENUM_CLASS_RANGE(World, FixedPixelSize)
    };

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    //   GetScaleModeName：枚举 → 名字（switch）
    //   ParseScaleMode：名字 → 枚举（线性查表）
    // 新增模式只需改枚举 + 本 switch（名字表随之自动覆盖）。
    // 2026-10-14（S3b-3）：原为 `MaterialDefinitionFile.cpp` 的本地单向表（名字只在那里活着）。
    inline constexpr uint32_t ScaleModeCount =
        static_cast<uint32_t>(ScaleMode::FixedPixelSize) + 1;

    inline const char *GetScaleModeName(const ScaleMode mode) noexcept
    {
        switch (mode)
        {
        case ScaleMode::World:               return "World";
        case ScaleMode::FixedPixelSize: return "FixedPixelSize";
        }
        return "World";
    }

    inline bool ParseScaleMode(const char *name, ScaleMode &out) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < ScaleModeCount; ++i)
        {
            const ScaleMode mode = static_cast<ScaleMode>(i);
            if (std::strcmp(name, GetScaleModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }


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

    // ── 名字注册表（单一真源；T1 口径：名字↔值只此一处）──────────────
    //   GetProjectionModeName：枚举 → 名字（switch）
    //   ParseProjectionMode：名字 → 枚举（线性查表）
    // 新增模式只需改枚举 + 本 switch（名字表随之自动覆盖）。
    // 2026-10-14（S3b-3）：原为 `MaterialDefinitionFile.cpp` 的本地单向表（名字只在那里活着）。
    inline constexpr uint32_t ProjectionModeCount =
        static_cast<uint32_t>(ProjectionMode::ClipPassthrough) + 1;

    inline const char *GetProjectionModeName(const ProjectionMode mode) noexcept
    {
        switch (mode)
        {
        case ProjectionMode::WorldCameraVP: return "WorldCameraVP";
        case ProjectionMode::LocalToWorldOnly: return "LocalToWorldOnly";
        case ProjectionMode::OrthoViewport: return "OrthoViewport";
        case ProjectionMode::OrthoThenLocalToWorld: return "OrthoThenLocalToWorld";
        case ProjectionMode::ClipPassthrough: return "ClipPassthrough";
        }
        return "WorldCameraVP";
    }

    inline bool ParseProjectionMode(const char *name, ProjectionMode &out) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32_t i = 0; i < ProjectionModeCount; ++i)
        {
            const ProjectionMode mode = static_cast<ProjectionMode>(i);
            if (std::strcmp(name, GetProjectionModeName(mode)) == 0)
            {
                out = mode;
                return true;
            }
        }
        return false;
    }


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
