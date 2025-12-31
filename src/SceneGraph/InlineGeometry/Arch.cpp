// Arch geometry generator for ULRE engine
// Creates an architectural arch (rounded or pointed)

#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateArch(GeometryCreater *pc, const ArchCreateInfo *aci)
    {
        if(!pc || !aci)
            return nullptr;

        const float width = aci->width;
        const float height = aci->height;
        const float depth = aci->depth;
        const float thickness = aci->thickness;
        const uint segments = std::max<uint>(4, aci->segments);
        const bool pointed = aci->pointed;

        // Validate parameters
        if(width <= 0.0f || height <= 0.0f || depth <= 0.0f || thickness <= 0.0f)
            return nullptr;

        if(thickness >= width * 0.5f)
            return nullptr;

        const float hw = width * 0.5f;
        const float hd = depth * 0.5f;
        const float inner_hw = hw - thickness;

        // Arch profile: half-circle or pointed arc
        // Vertices: segments + 1 points for outer arc, segments + 1 for inner arc
        // Times 2 for front and back faces
        const uint profile_verts = (segments + 1) * 2;  // outer + inner
        const uint face_verts = profile_verts * 2;  // front + back
        const uint side_verts = profile_verts * 2;  // top and bottom of extrusion

        const uint numberVertices = face_verts + side_verts + 200;  // Extra for rectangular base

        const uint numberIndices = segments * 12 + 200;  // Approximate

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Arch", numberVertices, numberIndices))
            return nullptr;

        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return nullptr;

        // Generate arch profile
        auto arch_point = [&](float t, float radius_scale) -> std::pair<float, float>
        {
            if(!pointed)
            {
                // Circular arch
                float angle = math::pi * t;  // 0 to pi
                float x = -cos(angle) * hw * radius_scale;
                float y = sin(angle) * height;
                return {x, y};
            }
            else
            {
                // Pointed arch (simplified Gothic)
                float angle = math::pi * t;
                float x = -cos(angle) * hw * radius_scale;
                float y_scale = (t < 0.5f) ? 1.0f + t : 2.0f - t;  // Peak in middle
                float y = sin(angle) * height * y_scale;
                return {x, y};
            }
        };

        // Front face - outer arc
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [x, y] = arch_point(t, 1.0f);

            builder.WriteFullVertex(x, y, hd,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   t, 1.0f);
        }

        // Front face - inner arc
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(segments - i) / float(segments);  // Reverse
            auto [x, y] = arch_point(t, (inner_hw / hw));

            builder.WriteFullVertex(x, y, hd,
                                   0.0f, 0.0f, 1.0f,
                                   1.0f, 0.0f, 0.0f,
                                   t, 0.0f);
        }

        // Back face - similar structure
        for(uint i = 0; i <= segments; i++)
        {
            float t = float(i) / float(segments);
            auto [x, y] = arch_point(t, 1.0f);

            builder.WriteFullVertex(x, y, -hd,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   t, 1.0f);
        }

        for(uint i = 0; i <= segments; i++)
        {
            float t = float(segments - i) / float(segments);
            auto [x, y] = arch_point(t, (inner_hw / hw));

            builder.WriteFullVertex(x, y, -hd,
                                   0.0f, 0.0f, -1.0f,
                                   -1.0f, 0.0f, 0.0f,
                                   t, 0.0f);
        }

        // Generate indices (simplified)
        const IndexType index_type = pc->GetIndexType();

        auto generate_indices = [&](auto *ip) -> void
        {
            using IndexT = typename std::remove_pointer<decltype(ip)>::type;

            // Front face
            for(uint i = 0; i < profile_verts - 1; i++)
            {
                *ip++ = 0;
                *ip++ = i + 1;
                *ip++ = i;
            }

            // Back face
            IndexT back_base = profile_verts;
            for(uint i = 0; i < profile_verts - 1; i++)
            {
                *ip++ = back_base;
                *ip++ = back_base + i;
                *ip++ = back_base + i + 1;
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

        Geometry *p = pc->Create();

        // Set bounding box
        BoundingVolumes bv;
        bv.SetFromAABB(math::Vector3f(-hw, 0.0f, -hd),
                       Vector3f(hw, height, hd));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace hgl::graph::inline_geometry
