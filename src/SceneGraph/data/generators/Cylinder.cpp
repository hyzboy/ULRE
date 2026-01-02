#include<hgl/graph/data/GeometryGenerators.h>
#include<cmath>
#include<numbers>

namespace hgl::graph::data
{
    using namespace hgl::math;

    GeometryDataPtr GenerateCylinder(const inline_geometry::CylinderCreateInfo &cci)
    {
        // Validate parameters
        if(cci.numberSlices < 3)
            return nullptr;

        auto geom = std::make_shared<GeometryData>();

        // Calculate vertex and index counts
        // Vertices: 1 bottom center + (slices+1) bottom ring + 1 top center + (slices+1) top ring + (slices+1)*2 side vertices
        uint32_t numberVertices = (cci.numberSlices + 2) * 2 + (cci.numberSlices + 1) * 2;
        uint32_t numberIndices = cci.numberSlices * 3 * 2 + cci.numberSlices * 6;

        // Pre-allocate space
        geom->vertices.reserve(numberVertices);
        geom->indices.reserve(numberIndices);

        // Set layout
        geom->layout.has_position = true;
        geom->layout.has_normal = true;
        geom->layout.has_tangent = true;
        geom->layout.has_texcoord = true;
        geom->topology = PrimitiveTopology::TriangleList;

        const float angleStep = (2.0f * std::numbers::pi_v<float>) / static_cast<float>(cci.numberSlices);

        Vertex v;

        // ========== Bottom cap vertices ==========
        // Bottom center vertex
        v.position = Vector3f(0.0f, 0.0f, -cci.halfExtend);
        v.normal = Vector3f(0.0f, 0.0f, -1.0f);
        v.tangent = Vector3f(0.0f, 1.0f, 0.0f);
        v.texcoord = Vector2f(0.5f, 0.5f);
        geom->vertices.push_back(v);

        // Bottom ring vertices (for bottom cap)
        for(uint32_t i = 0; i <= cci.numberSlices; i++)
        {
            const float currentAngle = angleStep * static_cast<float>(i);
            const float cosAngle = std::cos(currentAngle);
            const float sinAngle = std::sin(currentAngle);

            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, -cci.halfExtend);
            v.normal = Vector3f(0.0f, 0.0f, -1.0f);
            v.tangent = Vector3f(sinAngle, cosAngle, 0.0f);
            v.texcoord = Vector2f(static_cast<float>(i) / static_cast<float>(cci.numberSlices), 0.0f);
            geom->vertices.push_back(v);
        }

        // ========== Top cap vertices ==========
        // Top center vertex
        v.position = Vector3f(0.0f, 0.0f, cci.halfExtend);
        v.normal = Vector3f(0.0f, 0.0f, 1.0f);
        v.tangent = Vector3f(0.0f, -1.0f, 0.0f);
        v.texcoord = Vector2f(0.5f, 0.5f);
        geom->vertices.push_back(v);

        // Top ring vertices (for top cap)
        for(uint32_t i = 0; i <= cci.numberSlices; i++)
        {
            const float currentAngle = angleStep * static_cast<float>(i);
            const float cosAngle = std::cos(currentAngle);
            const float sinAngle = std::sin(currentAngle);

            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, cci.halfExtend);
            v.normal = Vector3f(0.0f, 0.0f, 1.0f);
            v.tangent = Vector3f(-sinAngle, -cosAngle, 0.0f);
            v.texcoord = Vector2f(static_cast<float>(i) / static_cast<float>(cci.numberSlices), 1.0f);
            geom->vertices.push_back(v);
        }

        // ========== Side vertices ==========
        // Generate vertices for cylinder sides (interleaved bottom-top pairs)
        for(uint32_t i = 0; i <= cci.numberSlices; i++)
        {
            const float currentAngle = angleStep * static_cast<float>(i);
            const float cosAngle = std::cos(currentAngle);
            const float sinAngle = std::sin(currentAngle);
            const float u = static_cast<float>(i) / static_cast<float>(cci.numberSlices);

            // Normal and tangent point radially outward for cylinder sides
            const Vector3f sideNormal(cosAngle, -sinAngle, 0.0f);
            const Vector3f sideTangent(-sinAngle, -cosAngle, 0.0f);

            // Bottom side vertex
            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, -cci.halfExtend);
            v.normal = sideNormal;
            v.tangent = sideTangent;
            v.texcoord = Vector2f(u, 0.0f);
            geom->vertices.push_back(v);

            // Top side vertex
            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, cci.halfExtend);
            v.normal = sideNormal;
            v.tangent = sideTangent;
            v.texcoord = Vector2f(u, 1.0f);
            geom->vertices.push_back(v);
        }

        // ========== Generate indices ==========
        uint32_t vertexIndex = 0;

        // Bottom cap indices (center at index 0, ring starts at index 1)
        const uint32_t bottomCenter = vertexIndex++;
        const uint32_t bottomRingStart = vertexIndex;
        vertexIndex += cci.numberSlices + 1;

        for(uint32_t i = 0; i < cci.numberSlices; i++)
        {
            geom->indices.push_back(bottomCenter);
            geom->indices.push_back(bottomRingStart + i);
            geom->indices.push_back(bottomRingStart + i + 1);
        }

        // Top cap indices (center comes next, then ring)
        const uint32_t topCenter = vertexIndex++;
        const uint32_t topRingStart = vertexIndex;
        vertexIndex += cci.numberSlices + 1;

        for(uint32_t i = 0; i < cci.numberSlices; i++)
        {
            geom->indices.push_back(topCenter);
            geom->indices.push_back(topRingStart + i + 1);
            geom->indices.push_back(topRingStart + i);
        }

        // Side indices (vertices are interleaved: bottom[i], top[i], bottom[i+1], top[i+1], ...)
        const uint32_t sideStart = vertexIndex;

        for(uint32_t i = 0; i < cci.numberSlices; i++)
        {
            const uint32_t v0 = sideStart + i * 2;       // bottom vertex at angle i
            const uint32_t v1 = sideStart + i * 2 + 1;   // top vertex at angle i
            const uint32_t v2 = sideStart + (i + 1) * 2; // bottom vertex at angle i+1
            const uint32_t v3 = sideStart + (i + 1) * 2 + 1; // top vertex at angle i+1

            // First triangle of quad (v0, v1, v2)
            geom->indices.push_back(v0);
            geom->indices.push_back(v1);
            geom->indices.push_back(v2);

            // Second triangle of quad (v2, v1, v3)
            geom->indices.push_back(v2);
            geom->indices.push_back(v1);
            geom->indices.push_back(v3);
        }

        // Calculate bounds
        geom->CalculateBounds();

        return geom;
    }
}
