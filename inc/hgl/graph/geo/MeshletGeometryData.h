#pragma once

#include <hgl/graph/ssbo/MeshletRows.h>
#include <hgl/type/ValueArray.h>
#include <cstdint>

namespace hgl::graph
{
    struct MeshletGeometryData
    {
        hgl::ValueArray<ssbo::MeshletDescriptorRow> descriptors;
        hgl::ValueArray<ssbo::MeshletBoundsRow> bounds;
        hgl::ValueArray<ssbo::MeshletVertexRow> vertices;
        hgl::ValueArray<ssbo::MeshletTriangleRow> triangles;

        void Clear()
        {
            descriptors.Clear();
            bounds.Clear();
            vertices.Clear();
            triangles.Clear();
        }

        bool IsValid() const
        {
            return descriptors.GetCount() > 0
                && descriptors.GetCount() == bounds.GetCount()
                && vertices.GetCount() > 0
                && triangles.GetCount() > 0;
        }
    };

    bool BuildMeshletGeometryData(const float *positions,
                                  uint32_t vertex_count,
                                  const uint32_t *indices,
                                  uint32_t index_count,
                                  MeshletGeometryData &out);
}
