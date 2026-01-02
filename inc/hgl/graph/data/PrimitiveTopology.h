#pragma once

namespace hgl::graph::data
{
    /**
     * 图元拓扑类型
     * Primitive topology type for geometry rendering
     */
    enum class PrimitiveTopology
    {
        PointList = 0,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,
        TriangleFan
    };
}
