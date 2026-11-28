#include <hgl/graph/geo/MeshletGeometryData.h>
#include <meshoptimizer.h>

namespace hgl::graph
{
    bool BuildMeshletGeometryData(const float *positions,
                                  const uint32_t vertex_count,
                                  const uint32_t *indices,
                                  const uint32_t index_count,
                                  MeshletGeometryData &out)
    {
        out.Clear();
        if (!positions || vertex_count == 0 || !indices
            || index_count < 3 || index_count % 3 != 0
            || index_count > 0x7fffffffu)
            return false;

        constexpr size_t kMaxVertices = 64;
        constexpr size_t kMaxTriangles = 124;
        constexpr float kConeWeight = 0.5f;

        const size_t max_meshlets =
            meshopt_buildMeshletsBound(index_count, kMaxVertices, kMaxTriangles);
        if (max_meshlets == 0
            || max_meshlets > static_cast<size_t>(0x7fffffffu))
            return false;

        hgl::ValueArray<meshopt_Meshlet> meshlets;
        hgl::ValueArray<unsigned int> meshlet_vertices;
        hgl::ValueArray<unsigned char> meshlet_triangles;
        if (!meshlets.Resize(static_cast<int>(max_meshlets))
            || !meshlet_vertices.Resize(static_cast<int>(index_count))
            || !meshlet_triangles.Resize(static_cast<int>(index_count)))
            return false;

        const size_t meshlet_count = meshopt_buildMeshlets(
            meshlets.GetData(),
            meshlet_vertices.GetData(),
            meshlet_triangles.GetData(),
            indices,
            index_count,
            positions,
            vertex_count,
            sizeof(float) * 3,
            kMaxVertices,
            kMaxTriangles,
            kConeWeight);
        if (meshlet_count == 0)
            return false;

        const meshopt_Meshlet &last = meshlets[static_cast<int>(meshlet_count - 1)];
        const size_t vertex_total = size_t(last.vertex_offset) + last.vertex_count;
        const size_t triangle_total =
            size_t(last.triangle_offset) + size_t(last.triangle_count) * 3;
        const size_t triangle_count = triangle_total / 3;

        if (triangle_total % 3 != 0
            || meshlet_count > static_cast<size_t>(0x7fffffffu)
            || vertex_total > static_cast<size_t>(0x7fffffffu)
            || triangle_count > static_cast<size_t>(0x7fffffffu))
            return false;

        if (!out.descriptors.Resize(static_cast<int>(meshlet_count))
            || !out.bounds.Resize(static_cast<int>(meshlet_count))
            || !out.vertices.Resize(static_cast<int>(vertex_total))
            || !out.triangles.Resize(static_cast<int>(triangle_count)))
        {
            out.Clear();
            return false;
        }

        for (size_t meshlet_index = 0; meshlet_index < meshlet_count; ++meshlet_index)
        {
            const meshopt_Meshlet &source = meshlets[static_cast<int>(meshlet_index)];
            if (source.vertex_count > kMaxVertices
                || source.triangle_count > kMaxTriangles
                || source.triangle_offset % 3 != 0)
            {
                out.Clear();
                return false;
            }

            ssbo::MeshletDescriptorRow &descriptor =
                out.descriptors[static_cast<int>(meshlet_index)];
            descriptor.vertex_offset = source.vertex_offset;
            descriptor.triangle_offset = source.triangle_offset / 3;
            descriptor.bounds_index = static_cast<uint32_t>(meshlet_index);
            descriptor.packed_counts =
                (source.vertex_count & 0xFFu) | ((source.triangle_count & 0xFFu) << 8);

            const meshopt_Bounds source_bounds = meshopt_computeMeshletBounds(
                meshlet_vertices.GetData() + source.vertex_offset,
                meshlet_triangles.GetData() + source.triangle_offset,
                source.triangle_count,
                positions,
                vertex_count,
                sizeof(float) * 3);

            ssbo::MeshletBoundsRow &bounds = out.bounds[static_cast<int>(meshlet_index)];
            bounds.center_radius =
                hgl::math::Vector4f(source_bounds.center[0],
                                    source_bounds.center[1],
                                    source_bounds.center[2],
                                    source_bounds.radius);
            bounds.cone_apex[0] = source_bounds.cone_apex[0];
            bounds.cone_apex[1] = source_bounds.cone_apex[1];
            bounds.cone_apex[2] = source_bounds.cone_apex[2];
            bounds.cone_axis_cutoff_packed =
                uint32_t(static_cast<uint8_t>(source_bounds.cone_axis_s8[0]))
                | (uint32_t(static_cast<uint8_t>(source_bounds.cone_axis_s8[1])) << 8)
                | (uint32_t(static_cast<uint8_t>(source_bounds.cone_axis_s8[2])) << 16)
                | (uint32_t(static_cast<uint8_t>(source_bounds.cone_cutoff_s8)) << 24);
        }

        for (size_t i = 0; i < vertex_total; ++i)
        {
            if (meshlet_vertices[static_cast<int>(i)] >= vertex_count)
            {
                out.Clear();
                return false;
            }
            out.vertices[static_cast<int>(i)].vertex_index = meshlet_vertices[static_cast<int>(i)];
        }

        for (size_t i = 0; i < triangle_count; ++i)
        {
            const size_t byte_index = i * 3;
            if (meshlet_triangles[static_cast<int>(byte_index)] >= kMaxVertices
                || meshlet_triangles[static_cast<int>(byte_index + 1)] >= kMaxVertices
                || meshlet_triangles[static_cast<int>(byte_index + 2)] >= kMaxVertices)
            {
                out.Clear();
                return false;
            }
            out.triangles[static_cast<int>(i)].packed_indices =
                uint32_t(meshlet_triangles[static_cast<int>(byte_index)])
                | (uint32_t(meshlet_triangles[static_cast<int>(byte_index + 1)]) << 8)
                | (uint32_t(meshlet_triangles[static_cast<int>(byte_index + 2)]) << 16);
        }

        return out.IsValid();
    }
}
