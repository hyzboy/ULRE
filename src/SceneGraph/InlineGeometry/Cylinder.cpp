#include "InlineGeometryCommon.h"
#include<hgl/graph/data/GeometryGenerators.h>
#include<hgl/graph/adapter/GeometryAdapter.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCylinder(GeometryCreater *pc,const CylinderCreateInfo *cci)
    {
        if(!pc || !cci)
            return nullptr;

        // Use new pure data generator
        auto geom_data = data::GenerateCylinder(*cci);
        if(!geom_data)
            return nullptr;

        // Convert to Vulkan Geometry through adapter
        return ToVulkanGeometry(*geom_data, pc, "Cylinder");
    }
} // namespace
