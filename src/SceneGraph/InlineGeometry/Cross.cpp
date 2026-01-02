// Cross geometry generator for ULRE engine
// Creates a 3D cross/plus shape

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCross(GeometryCreater *pc, const CrossCreateInfo *cci)
    {
        if(!pc || !cci)
            return nullptr;

        const float vertical_length = cci->vertical_length;
        const float horizontal_length = cci->horizontal_length;
        const float thickness = cci->thickness;
        const float depth = cci->depth;
        const float horizontal_offset = cci->horizontal_offset;

        // Validate parameters
        if(vertical_length <= 0.0f || horizontal_length <= 0.0f || 
           thickness <= 0.0f || depth <= 0.0f)
            return nullptr;

        if(horizontal_offset < 0.0f || horizontal_offset >= vertical_length)
            return nullptr;

        // Cross is composed of two rectangular beams:
        // 1. Vertical beam: thickness x vertical_length x depth
        // 2. Horizontal beam: horizontal_length x thickness x depth
        
        const float half_thickness = thickness * 0.5f;
        const float half_depth = depth * 0.5f;
        const float half_h_length = horizontal_length * 0.5f;

        // Vertical beam center is at origin
        // Horizontal beam is offset from top
        const float v_top = vertical_length * 0.5f;
        const float v_bottom = -vertical_length * 0.5f;
        const float h_center_y = v_top - horizontal_offset;

        // Each beam is a box with 6 faces
        // We need to handle the intersection area specially
        
        // For simplicity, generate two boxes and let them overlap
        // Vertical beam: 24 vertices (4 per face * 6 faces)
        // Horizontal beam: 24 vertices
        const uint numberVertices = 48;
        const uint numberIndices = 12 * 6; // 12 triangles per box

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Cross", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Helper to write a box face
        auto write_quad_face = [&](float x0, float y0, float z0,
                                   float x1, float y1, float z1,
                                   float x2, float y2, float z2,
                                   float x3, float y3, float z3,
                                   float nx, float ny, float nz)
        {
            // Tangent is along the first edge
            float tx = x1 - x0;
            float ty = y1 - y0;
            float tz = z1 - z0;
            float tlen = sqrtf(tx*tx + ty*ty + tz*tz);
            if(tlen > 0.0001f)
            {
                tx /= tlen;
                ty /= tlen;
                tz /= tlen;
            }

            if(builder.HasTangents())
            {
                builder.WriteFullVertex(x0, y0, z0, nx, ny, nz, tx, ty, tz, 0.0f, 0.0f);
                builder.WriteFullVertex(x1, y1, z1, nx, ny, nz, tx, ty, tz, 1.0f, 0.0f);
                builder.WriteFullVertex(x2, y2, z2, nx, ny, nz, tx, ty, tz, 1.0f, 1.0f);
                builder.WriteFullVertex(x3, y3, z3, nx, ny, nz, tx, ty, tz, 0.0f, 1.0f);
            }
            else
            {
                builder.WriteVertex(x0, y0, z0);
                builder.WriteNormal(nx, ny, nz);
                builder.WriteTexCoord(0.0f, 0.0f);

                builder.WriteVertex(x1, y1, z1);
                builder.WriteNormal(nx, ny, nz);
                builder.WriteTexCoord(1.0f, 0.0f);

                builder.WriteVertex(x2, y2, z2);
                builder.WriteNormal(nx, ny, nz);
                builder.WriteTexCoord(1.0f, 1.0f);

                builder.WriteVertex(x3, y3, z3);
                builder.WriteNormal(nx, ny, nz);
                builder.WriteTexCoord(0.0f, 1.0f);
            }
        };

        // Vertical beam corners
        const float vx0 = -half_thickness, vx1 = half_thickness;
        const float vy0 = v_bottom, vy1 = v_top;
        const float vz0 = -half_depth, vz1 = half_depth;

        // Vertical beam faces
        // Bottom face (Y-)
        write_quad_face(vx0, vy0, vz0,  vx1, vy0, vz0,  vx1, vy0, vz1,  vx0, vy0, vz1,  0.0f, -1.0f, 0.0f);
        // Top face (Y+)
        write_quad_face(vx0, vy1, vz1,  vx1, vy1, vz1,  vx1, vy1, vz0,  vx0, vy1, vz0,  0.0f, 1.0f, 0.0f);
        // Left face (X-)
        write_quad_face(vx0, vy0, vz1,  vx0, vy0, vz0,  vx0, vy1, vz0,  vx0, vy1, vz1,  -1.0f, 0.0f, 0.0f);
        // Right face (X+)
        write_quad_face(vx1, vy0, vz0,  vx1, vy0, vz1,  vx1, vy1, vz1,  vx1, vy1, vz0,  1.0f, 0.0f, 0.0f);
        // Back face (Z-)
        write_quad_face(vx1, vy0, vz0,  vx0, vy0, vz0,  vx0, vy1, vz0,  vx1, vy1, vz0,  0.0f, 0.0f, -1.0f);
        // Front face (Z+)
        write_quad_face(vx0, vy0, vz1,  vx1, vy0, vz1,  vx1, vy1, vz1,  vx0, vy1, vz1,  0.0f, 0.0f, 1.0f);

        // Horizontal beam corners
        const float hx0 = -half_h_length, hx1 = half_h_length;
        const float hy0 = h_center_y - half_thickness, hy1 = h_center_y + half_thickness;
        const float hz0 = -half_depth, hz1 = half_depth;

        // Horizontal beam faces
        // Bottom face (Y-)
        write_quad_face(hx0, hy0, hz0,  hx1, hy0, hz0,  hx1, hy0, hz1,  hx0, hy0, hz1,  0.0f, -1.0f, 0.0f);
        // Top face (Y+)
        write_quad_face(hx0, hy1, hz1,  hx1, hy1, hz1,  hx1, hy1, hz0,  hx0, hy1, hz0,  0.0f, 1.0f, 0.0f);
        // Left face (X-)
        write_quad_face(hx0, hy0, hz1,  hx0, hy0, hz0,  hx0, hy1, hz0,  hx0, hy1, hz1,  -1.0f, 0.0f, 0.0f);
        // Right face (X+)
        write_quad_face(hx1, hy0, hz0,  hx1, hy0, hz1,  hx1, hy1, hz1,  hx1, hy1, hz0,  1.0f, 0.0f, 0.0f);
        // Back face (Z-)
        write_quad_face(hx1, hy0, hz0,  hx0, hy0, hz0,  hx0, hy1, hz0,  hx1, hy1, hz0,  0.0f, 0.0f, -1.0f);
        // Front face (Z+)
        write_quad_face(hx0, hy0, hz1,  hx1, hy0, hz1,  hx1, hy1, hz1,  hx0, hy1, hz1,  0.0f, 0.0f, 1.0f);

        // Generate indices for quads (2 triangles per quad, 6 quads per box, 2 boxes)
        {
            const IndexType index_type = pc->GetIndexType();
            
            auto generate_indices = [&](auto *ip)
            {
                for(uint box = 0; box < 2; box++)
                {
                    uint base = box * 24;
                    for(uint face = 0; face < 6; face++)
                    {
                        uint face_base = base + face * 4;
                        // First triangle
                        *ip++ = face_base + 0;
                        *ip++ = face_base + 1;
                        *ip++ = face_base + 2;
                        // Second triangle
                        *ip++ = face_base + 0;
                        *ip++ = face_base + 2;
                        *ip++ = face_base + 3;
                    }
                }
            };

            if(index_type == IndexType::U16)
            {
                IBTypeMap<uint16> ib(pc->GetIBMap());
                uint16 *ip = ib;
                generate_indices(ip);
            }
            else if(index_type == IndexType::U32)
            {
                IBTypeMap<uint32> ib(pc->GetIBMap());
                uint32 *ip = ib;
                generate_indices(ip);
            }
            else if(index_type == IndexType::U8)
            {
                IBTypeMap<uint8> ib(pc->GetIBMap());
                uint8 *ip = ib;
                generate_indices(ip);
            }
            else
                return nullptr;
        }

        // Create geometry and set bounding box
        return pc->CreateWithAABB(
            Vector3f(-half_h_length, v_bottom, -half_depth),
            Vector3f(half_h_length, v_top, half_depth));
    }
}//namespace hgl::graph::inline_geometry
