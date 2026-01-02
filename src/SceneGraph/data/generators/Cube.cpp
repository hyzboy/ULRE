#include<hgl/graph/data/GeometryGenerators.h>

namespace hgl::graph::data
{
    using namespace hgl::math;

    GeometryDataPtr GenerateCube(const inline_geometry::CubeCreateInfo &cci)
    {
        auto geom = std::make_shared<GeometryData>();
        
        // Cube has 24 vertices (4 per face, 6 faces) and 36 indices (2 triangles per face)
        geom->vertices.reserve(24);
        geom->indices.reserve(36);

        // Set layout based on creation info
        geom->layout.has_position = true;
        geom->layout.has_normal = cci.normal;
        geom->layout.has_tangent = cci.tangent;
        geom->layout.has_texcoord = cci.tex_coord;
        geom->layout.has_color = (cci.color_type != inline_geometry::CubeCreateInfo::ColorType::NoColor);
        geom->topology = PrimitiveTopology::TriangleList;

        // Define cube vertex positions
        constexpr float positions[][3] = {
            // Bottom face (Y-)
            {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, -0.5f, -0.5f},
            // Top face (Y+)
            {-0.5f, +0.5f, -0.5f}, {-0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, -0.5f},
            // Back face (Z-)
            {-0.5f, -0.5f, -0.5f}, {-0.5f, +0.5f, -0.5f}, {+0.5f, +0.5f, -0.5f}, {+0.5f, -0.5f, -0.5f},
            // Front face (Z+)
            {-0.5f, -0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {+0.5f, -0.5f, +0.5f},
            // Left face (X-)
            {-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, +0.5f}, {-0.5f, +0.5f, +0.5f}, {-0.5f, +0.5f, -0.5f},
            // Right face (X+)
            {+0.5f, -0.5f, -0.5f}, {+0.5f, -0.5f, +0.5f}, {+0.5f, +0.5f, +0.5f}, {+0.5f, +0.5f, -0.5f}
        };

        constexpr float normals[][3] = {
            {+0.0f, +1.0f, +0.0f}, {+0.0f, +1.0f, +0.0f}, {+0.0f, +1.0f, +0.0f}, {+0.0f, +1.0f, +0.0f},
            {+0.0f, -1.0f, +0.0f}, {+0.0f, -1.0f, +0.0f}, {+0.0f, -1.0f, +0.0f}, {+0.0f, -1.0f, +0.0f},
            {+0.0f, -0.0f, -1.0f}, {+0.0f, -0.0f, -1.0f}, {+0.0f, -0.0f, -1.0f}, {+0.0f, -0.0f, -1.0f},
            {+0.0f, -0.0f, +1.0f}, {+0.0f, -0.0f, +1.0f}, {+0.0f, -0.0f, +1.0f}, {+0.0f, -0.0f, +1.0f},
            {-1.0f, -0.0f, +0.0f}, {-1.0f, -0.0f, +0.0f}, {-1.0f, -0.0f, +0.0f}, {-1.0f, -0.0f, +0.0f},
            {+1.0f, -0.0f, +0.0f}, {+1.0f, -0.0f, +0.0f}, {+1.0f, -0.0f, +0.0f}, {+1.0f, -0.0f, +0.0f}
        };

        constexpr float tangents[][3] = {
            {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f},
            {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
            {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {+1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, {0.0f, 0.0f, +1.0f}, {0.0f, 0.0f, +1.0f},
            {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, -1.0f}
        };

        constexpr float tex_coords[][2] = {
            {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
            {0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
            {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f},
            {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
            {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f},
            {1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}
        };

        // Generate vertices
        for(int i = 0; i < 24; i++)
        {
            Vertex v;
            
            // Position
            v.position.x = positions[i][0];
            v.position.y = positions[i][1];
            v.position.z = positions[i][2];
            
            // Normal
            if(cci.normal)
            {
                v.normal.x = normals[i][0];
                v.normal.y = normals[i][1];
                v.normal.z = normals[i][2];
            }
            
            // Tangent
            if(cci.tangent)
            {
                v.tangent.x = tangents[i][0];
                v.tangent.y = tangents[i][1];
                v.tangent.z = tangents[i][2];
            }
            
            // Texture coordinates
            if(cci.tex_coord)
            {
                v.texcoord.x = tex_coords[i][0];
                v.texcoord.y = tex_coords[i][1];
            }
            
            // Color handling
            if(cci.color_type == inline_geometry::CubeCreateInfo::ColorType::SameColor)
            {
                v.color = cci.color[0];
            }
            else if(cci.color_type == inline_geometry::CubeCreateInfo::ColorType::FaceColor)
            {
                int face = i / 4;
                v.color = cci.color[face];
            }
            else if(cci.color_type == inline_geometry::CubeCreateInfo::ColorType::VertexColor)
            {
                v.color = cci.color[i];
            }
            
            geom->vertices.push_back(v);
        }

        // Generate indices (6 faces * 2 triangles * 3 vertices = 36 indices)
        constexpr uint32_t indices[] = {
            0,  2,  1,   0,  3,  2,   // Bottom
            4,  5,  6,   4,  6,  7,   // Top
            8,  9, 10,   8, 10, 11,   // Back
            12, 15, 14,  12, 14, 13,  // Front
            16, 17, 18,  16, 18, 19,  // Left
            20, 23, 22,  20, 22, 21   // Right
        };
        
        for(auto idx : indices)
        {
            geom->indices.push_back(idx);
        }

        // Calculate bounds
        geom->CalculateBounds();

        return geom;
    }
}
