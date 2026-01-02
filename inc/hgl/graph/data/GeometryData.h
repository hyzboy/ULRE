#pragma once
#include<hgl/graph/data/Vertex.h>
#include<hgl/graph/data/VertexLayout.h>
#include<hgl/graph/data/PrimitiveTopology.h>
#include<hgl/math/geometry/AABB.h>
#include<vector>
#include<memory>

namespace hgl::graph::data
{
    using namespace hgl::math;

    /**
     * 几何体数据（纯数据，无渲染依赖）
     * Geometry data class (pure data, no rendering dependencies)
     */
    class GeometryData
    {
    public:
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        VertexLayout layout;
        AABB bounds;
        PrimitiveTopology topology = PrimitiveTopology::TriangleList;

    public:
        GeometryData() = default;
        ~GeometryData() = default;

        /**
         * 计算包围盒
         * Calculate axis-aligned bounding box
         */
        void CalculateBounds();

        /**
         * 计算法线（基于三角形面法线平均）
         * Calculate normals (based on triangle face normal averaging)
         */
        void CalculateNormals();

        /**
         * 验证数据有效性
         * Validate geometry data
         */
        bool Validate() const;
        
        /**
         * 获取顶点数量
         * Get vertex count
         */
        size_t GetVertexCount() const { return vertices.size(); }

        /**
         * 获取索引数量
         * Get index count
         */
        size_t GetIndexCount() const { return indices.size(); }

        /**
         * 获取三角形数量
         * Get triangle count
         */
        size_t GetTriangleCount() const;
    };

    /**
     * 几何体数据智能指针类型
     * Smart pointer type for geometry data
     */
    using GeometryDataPtr = std::shared_ptr<GeometryData>;
}
