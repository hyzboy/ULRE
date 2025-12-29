#include<hgl/ecs/RenderItem.h>
#include<hgl/graph/mesh/Primitive.h>

namespace hgl::ecs
{
    // RenderItem base class implementation
    int RenderItem::Compare(const RenderItem& other) const
    {
        // Compare by primitive geometry first (for batching)
        auto* prim1 = GetPrimitive();
        auto* prim2 = other.GetPrimitive();
        
        if (prim1 && prim2)
        {
            // Compare geometry pointers for batching efficiency
            auto* geom1 = prim1->GetGeometry();
            auto* geom2 = prim2->GetGeometry();
            
            if (geom1 < geom2) return -1;
            if (geom1 > geom2) return 1;
        }
        
        // Then compare by distance to camera
        float diff = other.distanceToCamera - distanceToCamera;
        if (diff > 0.0f) return 1;
        if (diff < 0.0f) return -1;
        
        return 0;
    }
}//namespace hgl::ecs
