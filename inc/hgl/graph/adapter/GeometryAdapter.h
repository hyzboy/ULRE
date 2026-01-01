#pragma once
#include<hgl/graph/VK.h>
#include<hgl/graph/data/GeometryData.h>

namespace hgl::graph
{
    /**
     * 将 GeometryData 转换为 Vulkan Geometry
     * Convert GeometryData to Vulkan Geometry
     * 
     * @param geom_data 纯数据几何体 / Pure data geometry
     * @param pc GeometryCreater
     * @param name 几何体名称 / Geometry name
     * @return Vulkan Geometry 对象，失败返回 nullptr / Vulkan Geometry object, nullptr on failure
     */
    Geometry* ToVulkanGeometry(
        const data::GeometryData &geom_data,
        GeometryCreater *pc,
        const AnsiString &name);
}
