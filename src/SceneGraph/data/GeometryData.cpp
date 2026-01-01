#include<hgl/graph/data/GeometryData.h>
#include<algorithm>
#include<cmath>

namespace hgl::graph::data
{
    void GeometryData::CalculateBounds()
    {
        if(vertices.empty())
            return;

        Vector3f min = vertices[0].position;
        Vector3f max = vertices[0].position;

        for(const auto &v : vertices)
        {
            min.x = std::min(min.x, v.position.x);
            min.y = std::min(min.y, v.position.y);
            min.z = std::min(min.z, v.position.z);
            
            max.x = std::max(max.x, v.position.x);
            max.y = std::max(max.y, v.position.y);
            max.z = std::max(max.z, v.position.z);
        }

        bounds.minPoint = min;
        bounds.maxPoint = max;
    }

    void GeometryData::CalculateNormals()
    {
        if(topology != PrimitiveTopology::TriangleList)
            return;
        
        if(indices.empty() || vertices.empty())
            return;

        // Reset all normals to zero
        for(auto &v : vertices)
            v.normal = Vector3f(0, 0, 0);

        // Accumulate face normals to vertices
        for(size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            if(i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                continue;

            const Vector3f &p0 = vertices[i0].position;
            const Vector3f &p1 = vertices[i1].position;
            const Vector3f &p2 = vertices[i2].position;

            // Calculate face normal using cross product
            Vector3f edge1 = p1 - p0;
            Vector3f edge2 = p2 - p0;
            Vector3f normal = edge1.Cross(edge2);

            vertices[i0].normal += normal;
            vertices[i1].normal += normal;
            vertices[i2].normal += normal;
        }

        // Normalize all normals
        for(auto &v : vertices)
        {
            float len = std::sqrt(v.normal.x * v.normal.x + 
                                 v.normal.y * v.normal.y + 
                                 v.normal.z * v.normal.z);
            if(len > 0.0001f)
            {
                v.normal.x /= len;
                v.normal.y /= len;
                v.normal.z /= len;
            }
        }
    }

    bool GeometryData::Validate() const
    {
        if(vertices.empty())
            return false;

        // Check index validity
        for(auto idx : indices)
        {
            if(idx >= vertices.size())
                return false;
        }

        // Check topology constraints
        if(topology == PrimitiveTopology::TriangleList)
        {
            if(indices.size() % 3 != 0)
                return false;
        }
        else if(topology == PrimitiveTopology::LineList)
        {
            if(indices.size() % 2 != 0)
                return false;
        }

        return true;
    }

    size_t GeometryData::GetTriangleCount() const
    {
        if(topology == PrimitiveTopology::TriangleList)
            return indices.size() / 3;
        else if(topology == PrimitiveTopology::TriangleStrip && indices.size() >= 3)
            return indices.size() - 2;
        else if(topology == PrimitiveTopology::TriangleFan && indices.size() >= 3)
            return indices.size() - 2;
        
        return 0;
    }
}
