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

        float angleStep = (2.0f * std::numbers::pi_v<float>) / static_cast<float>(cci.numberSlices);

        // Bottom center vertex
        Vertex v;
        v.position = Vector3f(0.0f, 0.0f, -cci.halfExtend);
        v.normal = Vector3f(0.0f, 0.0f, -1.0f);
        v.tangent = Vector3f(0.0f, 1.0f, 0.0f);
        v.texcoord = Vector2f(0.0f, 0.0f);
        geom->vertices.push_back(v);

        // Bottom ring vertices
        for(uint32_t i = 0; i <= cci.numberSlices; i++)
        {
            float currentAngle = angleStep * static_cast<float>(i);
            float cosAngle = std::cos(currentAngle);
            float sinAngle = std::sin(currentAngle);

            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, -cci.halfExtend);
            v.normal = Vector3f(0.0f, 0.0f, -1.0f);
            v.tangent = Vector3f(sinAngle, cosAngle, 0.0f);
            v.texcoord = Vector2f(0.0f, 0.0f);
            geom->vertices.push_back(v);
        }

        // Top center vertex
        v.position = Vector3f(0.0f, 0.0f, cci.halfExtend);
        v.normal = Vector3f(0.0f, 0.0f, 1.0f);
        v.tangent = Vector3f(0.0f, -1.0f, 0.0f);
        v.texcoord = Vector2f(1.0f, 1.0f);
        geom->vertices.push_back(v);

        // Top ring vertices
        for(uint32_t i = 0; i <= cci.numberSlices; i++)
        {
            float currentAngle = angleStep * static_cast<float>(i);
            float cosAngle = std::cos(currentAngle);
            float sinAngle = std::sin(currentAngle);

            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, cci.halfExtend);
            v.normal = Vector3f(0.0f, 0.0f, 1.0f);
            v.tangent = Vector3f(-sinAngle, -cosAngle, 0.0f);
            v.texcoord = Vector2f(1.0f, 1.0f);
            geom->vertices.push_back(v);
        }

        // Side vertices (two rings for proper normals)
        for(uint32_t i = 0; i <= cci.numberSlices; i++)
        {
            float currentAngle = angleStep * static_cast<float>(i);
            float cosAngle = std::cos(currentAngle);
            float sinAngle = std::sin(currentAngle);

            // Bottom side vertex
            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, -cci.halfExtend);
            v.normal = Vector3f(cosAngle, -sinAngle, 0.0f);
            v.tangent = Vector3f(-sinAngle, -cosAngle, 0.0f);
            v.texcoord = Vector2f(static_cast<float>(i) / static_cast<float>(cci.numberSlices), 0.0f);
            geom->vertices.push_back(v);

            // Top side vertex
            v.position = Vector3f(cosAngle * cci.radius, -sinAngle * cci.radius, cci.halfExtend);
            v.normal = Vector3f(cosAngle, -sinAngle, 0.0f);
            v.tangent = Vector3f(-sinAngle, -cosAngle, 0.0f);
            v.texcoord = Vector2f(static_cast<float>(i) / static_cast<float>(cci.numberSlices), 1.0f);
            geom->vertices.push_back(v);
        }

        // Generate indices
        uint32_t centerIndex = 0;
        uint32_t indexCounter = 1;

        // Bottom cap indices
        for(uint32_t i = 0; i < cci.numberSlices; i++)
        {
            geom->indices.push_back(centerIndex);
            geom->indices.push_back(indexCounter);
            geom->indices.push_back(indexCounter + 1);
            indexCounter++;
        }
        indexCounter++;

        // Top cap indices
        centerIndex = indexCounter;
        indexCounter++;

        for(uint32_t i = 0; i < cci.numberSlices; i++)
        {
            geom->indices.push_back(centerIndex);
            geom->indices.push_back(indexCounter + 1);
            geom->indices.push_back(indexCounter);
            indexCounter++;
        }
        indexCounter++;

        // Side indices
        for(uint32_t i = 0; i < cci.numberSlices; i++)
        {
            geom->indices.push_back(indexCounter);
            geom->indices.push_back(indexCounter + 1);
            geom->indices.push_back(indexCounter + 2);

            geom->indices.push_back(indexCounter + 2);
            geom->indices.push_back(indexCounter + 1);
            geom->indices.push_back(indexCounter + 3);

            indexCounter += 2;
        }

        // Calculate bounds
        geom->CalculateBounds();

        return geom;
    }
}
