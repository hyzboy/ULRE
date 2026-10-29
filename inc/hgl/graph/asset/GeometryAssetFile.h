#pragma once

#include <hgl/graph/asset/GeometryAssetSpec.h>

namespace hgl::graph
{
    /**
     * @brief 从 TOML 文件加载几何体资产规约
     *
     * 结构示例：
     * ```toml
     * schema = 1
     * name = "Sphere64"
     * type = "builtin"
     * shape = "sphere"
     *
     * [builtin.sphere]
     * slices = 64
     * ```
     */
    bool LoadGeometryAssetSpecFromTOML(const OSString &filename, GeometryAssetSpec &out_spec);

    /**
     * @brief 从内存字符串解析几何体资产规约
     */
    bool ParseGeometryAssetSpec(const char *content, size_t size, const os_char *debug_name, GeometryAssetSpec &out_spec);

}//namespace hgl::graph
