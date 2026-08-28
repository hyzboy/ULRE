#pragma once

#include <hgl/CoreType.h>
#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    enum class PrimitiveType:uint32
    {
        Points=0,
        Lines,
        LineStrip,
        Triangles,
        // Fan / TriangleStrip：仅 GLTFConvert 导入侧仍在产出（未来 glTF 重写时
        // 一并删除）——mesh 渲染路径不支持（mesh shader 输出恒 triangle list，
        // fan/strip 装配规则依赖连续顶点流，mesh 分组模型下跨组必然错，T3.5 已删
        // 生成侧模拟）。CPU 几何应显式转成 triangle list + IBO。
        TriangleStrip,
        Fan,

        ENUM_CLASS_RANGE(Points,Fan),

        Error
    };
}
