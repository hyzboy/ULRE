#pragma once
#include<cstddef>

namespace hgl::graph::data
{
    /**
     * 顶点布局描述
     * Vertex layout descriptor for geometry data
     */
    struct VertexLayout
    {
        bool has_position = true;   // Position is mandatory
        bool has_normal = false;
        bool has_tangent = false;
        bool has_texcoord = false;
        bool has_color = false;

        /**
         * 计算单个顶点的字节数
         * Calculate size of a single vertex in bytes
         */
        size_t GetVertexSize() const;

        /**
         * 检查两个布局是否兼容
         * Check if two layouts are compatible
         */
        bool IsCompatible(const VertexLayout &other) const;
    };
}
