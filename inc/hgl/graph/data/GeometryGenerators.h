#pragma once
#include<hgl/graph/data/GeometryData.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<cstdint>

namespace hgl::graph::data
{
    /**
     * 生成立方体几何数据
     * Generate cube geometry data
     * @param cci 立方体创建信息 / Cube creation info
     * @return 几何体数据，失败返回 nullptr / Geometry data, nullptr on failure
     */
    GeometryDataPtr GenerateCube(const inline_geometry::CubeCreateInfo &cci);

    /**
     * 生成圆柱体几何数据
     * Generate cylinder geometry data
     * @param cci 圆柱体创建信息 / Cylinder creation info
     * @return 几何体数据，失败返回 nullptr / Geometry data, nullptr on failure
     */
    GeometryDataPtr GenerateCylinder(const inline_geometry::CylinderCreateInfo &cci);
}
