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
        auto write_quad_face = [&](const Vector3f &v0, const Vector3f &v1, 
                                   const Vector3f &v2, const Vector3f &v3,
                                   const Vector3f &normal)
        {
            // Calculate UVs based on position
            builder.WriteFullVertex(v0, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.0f, 0.0f);
            builder.WriteFullVertex(v1, normal, Vector3f(1.0f, 0.0f, 0.0f), 1.0f, 0.0f);
            builder.WriteFullVertex(v2, normal, Vector3f(1.0f, 0.0f, 0.0f), 1.0f, 1.0f);
            builder.WriteFullVertex(v3, normal, Vector3f(1.0f, 0.0f, 0.0f), 0.0f, 1.0f);
        };

        // Vertical beam vertices (centered on Y axis)
        Vector3f v_corners[8] = {
            Vector3f(-half_thickness, v_bottom, -half_depth), // 0: bottom-left-back
            Vector3f(half_thickness, v_bottom, -half_depth),  // 1: bottom-right-back
            Vector3f(half_thickness, v_bottom, half_depth),   // 2: bottom-right-front
            Vector3f(-half_thickness, v_bottom, half_depth),  // 3: bottom-left-front
            Vector3f(-half_thickness, v_top, -half_depth),    // 4: top-left-back
            Vector3f(half_thickness, v_top, -half_depth),     // 5: top-right-back
            Vector3f(half_thickness, v_top, half_depth),      // 6: top-right-front
            Vector3f(-half_thickness, v_top, half_depth)      // 7: top-left-front
        };

        // Vertical beam faces
        write_quad_face(v_corners[0], v_corners[1], v_corners[2], v_corners[3], Vector3f(0.0f, -1.0f, 0.0f)); // Bottom
        write_quad_face(v_corners[4], v_corners[7], v_corners[6], v_corners[5], Vector3f(0.0f, 1.0f, 0.0f));  // Top
        write_quad_face(v_corners[0], v_corners[3], v_corners[7], v_corners[4], Vector3f(-1.0f, 0.0f, 0.0f)); // Left
        write_quad_face(v_corners[1], v_corners[5], v_corners[6], v_corners[2], Vector3f(1.0f, 0.0f, 0.0f));  // Right
        write_quad_face(v_corners[0], v_corners[4], v_corners[5], v_corners[1], Vector3f(0.0f, 0.0f, -1.0f)); // Back
        write_quad_face(v_corners[3], v_corners[2], v_corners[6], v_corners[7], Vector3f(0.0f, 0.0f, 1.0f));  // Front

        // Horizontal beam vertices (centered on h_center_y)
        Vector3f h_corners[8] = {
            Vector3f(-half_h_length, h_center_y - half_thickness, -half_depth), // 0: left-bottom-back
            Vector3f(half_h_length, h_center_y - half_thickness, -half_depth),  // 1: right-bottom-back
            Vector3f(half_h_length, h_center_y - half_thickness, half_depth),   // 2: right-bottom-front
            Vector3f(-half_h_length, h_center_y - half_thickness, half_depth),  // 3: left-bottom-front
            Vector3f(-half_h_length, h_center_y + half_thickness, -half_depth), // 4: left-top-back
            Vector3f(half_h_length, h_center_y + half_thickness, -half_depth),  // 5: right-top-back
            Vector3f(half_h_length, h_center_y + half_thickness, half_depth),   // 6: right-top-front
            Vector3f(-half_h_length, h_center_y + half_thickness, half_depth)   // 7: left-top-front
        };

        // Horizontal beam faces
        write_quad_face(h_corners[0], h_corners[1], h_corners[2], h_corners[3], Vector3f(0.0f, -1.0f, 0.0f)); // Bottom
        write_quad_face(h_corners[4], h_corners[7], h_corners[6], h_corners[5], Vector3f(0.0f, 1.0f, 0.0f));  // Top
        write_quad_face(h_corners[0], h_corners[3], h_corners[7], h_corners[4], Vector3f(-1.0f, 0.0f, 0.0f)); // Left
        write_quad_face(h_corners[1], h_corners[5], h_corners[6], h_corners[2], Vector3f(1.0f, 0.0f, 0.0f));  // Right
        write_quad_face(h_corners[0], h_corners[4], h_corners[5], h_corners[1], Vector3f(0.0f, 0.0f, -1.0f)); // Back
        write_quad_face(h_corners[3], h_corners[2], h_corners[6], h_corners[7], Vector3f(0.0f, 0.0f, 1.0f));  // Front

        // Generate indices for quads (2 triangles per quad, 6 quads per box, 2 boxes)
        uint idx = 0;
        for(uint box = 0; box < 2; box++)
        {
            uint base = box * 24;
            for(uint face = 0; face < 6; face++)
            {
                uint face_base = base + face * 4;
                // First triangle
                builder.WriteIndex(idx++, face_base + 0);
                builder.WriteIndex(idx++, face_base + 1);
                builder.WriteIndex(idx++, face_base + 2);
                // Second triangle
                builder.WriteIndex(idx++, face_base + 0);
                builder.WriteIndex(idx++, face_base + 2);
                builder.WriteIndex(idx++, face_base + 3);
            }
        }

        // Set bounding box
        Geometry *p = pc->End();
        if(p)
        {
            BoundingVolumes bv;
            bv.SetFromAABB(Vector3f(-half_h_length, v_bottom, -half_depth),
                          Vector3f(half_h_length, v_top, half_depth));
            p->SetBoundingVolumes(bv);
        }

        return p;
    }
}//namespace hgl::graph::inline_geometry
