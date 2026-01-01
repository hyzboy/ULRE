#include "InlineGeometryCommon.h"
#include<hgl/graph/data/GeometryGenerators.h>
#include<hgl/graph/adapter/GeometryAdapter.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCube(GeometryCreater *pc,const CubeCreateInfo *cci)
    {
        if(!pc || !cci)
            return nullptr;

        // Use new pure data generator
        auto geom_data = data::GenerateCube(*cci);
        if(!geom_data)
            return nullptr;

        // Convert to Vulkan Geometry through adapter
        return ToVulkanGeometry(*geom_data, pc, "Cube");
    }
} // namespace
