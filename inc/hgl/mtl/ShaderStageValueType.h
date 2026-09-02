#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>

namespace hgl::graph::mtl
{
    // 2026-09 死代码清扫：本头原名 ShaderStageBuildContext.h，曾承载
    // ShaderStageBuildContext 结构家族（stage key 测试辅助）——生产零消费，
    // 已迁至 src/Tools/ShaderGen/StageBuildContextTest.h（回归门专用）。
    // 生产唯一必需物 = 输出附件值类型枚举。

    enum class ShaderStageValueType : uint32
    {
        Unknown = 0,
        Float,
        Vec2,
        Vec3,
        Vec4,
        Int,
        UInt,
        Bool
    };
}
