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

    /**
     * @brief 装载 `.scene` 包为「图元资产 + 场景节点树」。
     *
     * 变换真源约定（T5）：`out_nodes` 每个节点的**局部 TRS**（`hasTRS`/
     * `translation`/`rotation`/`scale`）与父子关系（`parentIndex`/`children`）
     * 是 GLTFConvert 直接导出的无损数据；`localMatrix`/`worldMatrix` 只是
     * `ComposeNodeMatrices` 顺带组合出的派生便利值。
     *
     * 因此装载 ECS 时**不要**对 `worldMatrix` 做 `DecomposeTransform` 再摊平
     * 层级 —— 应使用 `StaticMeshSceneInstance::Instantiate`，它按局部 TRS +
     * `SetParent` 逐节点建实体，完整保留层级（且自持资产副本，指针不会悬垂）。
     *
     * 多材质（可选）：`material_recipes` 为按源场景材质下标索引的配方表
     * （glTF `materials[]` 顺序）。每个图元按其 `material_index` 选取配方；
     * 下标缺失/越界/条目为空时回退到 `recipe`。不传（默认）则整场景共用
     * `recipe`，与既有单材质行为完全一致。选中的材质下标会随
     * `PrimitiveAsset::GetMaterialIndex()` 透给实例化层。
     */
    bool LoadStaticMeshSceneAsPrimitiveAssets(
        VulkanDevice                     *device,
        GeometryManager                  *geo_mgr,
        const GeometryVertexFormat       &geometry_vertex_format,
        const mtl::MaterialRecipe        *recipe,
        const OSString                   &pack_path,
        const OSString                   &base_dir,
        std::vector<PrimitiveAsset>      &out_assets,
        std::vector<StaticMeshNode>      &out_nodes,
        std::vector<int32_t>             &out_root_nodes,
        const mtl::MaterialRecipe *const *material_recipes = nullptr,
        const uint32_t                    material_recipe_count = 0);

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
     * @note `material_recipes`/`material_recipe_count` 语义与
     *       `LoadStaticMeshSceneAsPrimitiveAssets` 相同（可选多材质表）。
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
        std::vector<int32_t>             &out_root_nodes,
        const mtl::MaterialRecipe *const *material_recipes = nullptr,
        const uint32_t                    material_recipe_count = 0);

}//namespace hgl::graph
