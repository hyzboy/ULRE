#pragma once

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/type/String.h>
#include <cstdint>

namespace hgl
{
    class VulkanDevice;
}

namespace hgl::graph
{
    Geometry *LoadGeometry(VulkanDevice *device,
                           const GeometryVertexFormat &geometry_vertex_format,
                           const OSString &filename);

    Geometry *LoadGeometryFromMiniPackBytes(VulkanDevice *device,
                                            const GeometryVertexFormat &geometry_vertex_format,
                                            const void *bytes,
                                            uint32 size,
                                            const OSString &debug_name);
}
