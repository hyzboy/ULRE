#pragma once

#include <hgl/type/String.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <vector>

namespace hgl::graph
{
    class VulkanDevice;
    class GeometryManager;
    class GeometryVertexFormat;

    bool LoadStaticMeshSceneAsPrimitiveAssets(
        VulkanDevice                     *device,
        GeometryManager                  *geo_mgr,
        const GeometryVertexFormat       &geometry_vertex_format,
        const mtl::MaterialRecipe        *recipe,
        const OSString                   &pack_path,
        const OSString                   &base_dir,
        std::vector<PrimitiveAsset>      &out_assets,
        std::vector<StaticMeshNode>      &out_nodes,
        std::vector<int32_t>             &out_root_nodes);

    /**
     * @brief 从**已读入内存**的 `.scene` 字节构造场景（异步加载路径）。
     *
     * 与 `LoadStaticMeshSceneAsPrimitiveAssets` 解析逻辑完全一致，只是数据来源从
     * 磁盘映射换成了内存块 —— 因此可以把「读盘」搬到后台 IO 线程，主线程只在
     * 数据就绪后做一次解析与 GPU 建 buffer。
     *
     * @param pack_bytes 完整场景包字节（本调用期间必须有效，不会被保留）
     * @param pack_name  仅用于日志/诊断的资产名
     * @note 这里创建的 `Geometry*` 会 `Add` 进 `geo_mgr`，与同步路径一致。
     */
    bool LoadStaticMeshSceneFromMemory(
        VulkanDevice                     *device,
        GeometryManager                  *geo_mgr,
        const GeometryVertexFormat       &geometry_vertex_format,
        const mtl::MaterialRecipe        *recipe,
        const void                       *pack_bytes,
        const uint32                      pack_size,
        const OSString                   &pack_name,
        const OSString                   &base_dir,
        std::vector<PrimitiveAsset>      &out_assets,
        std::vector<StaticMeshNode>      &out_nodes,
        std::vector<int32_t>             &out_root_nodes);

}//namespace hgl::graph
