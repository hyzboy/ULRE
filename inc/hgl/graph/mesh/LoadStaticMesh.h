#pragma once
#include <hgl/type/String.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <vector>
namespace hgl
{
    namespace io
    {
        class InputStream;
    }//namespace io
}
namespace hgl::graph
{
    class VulkanDevice;
    class GeometryManager;
    class GeometryVertexFormat;
    class GraphicsContext;

    /**
     * @brief 装载 `.model` 包为「图元资产 + 场景节点树」。
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
    bool LoadModelAsPrimitiveAssets(
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
     * @brief 从**已读入内存**的 `.model` 字节构造场景（异步加载路径）。
     *
     * 与 `LoadModelAsPrimitiveAssets` 解析逻辑完全一致，只是数据来源从
     * 磁盘映射换成了内存块 —— 因此可以把「读盘」搬到后台 IO 线程，主线程只在
     * 数据就绪后做一次解析与 GPU 建 buffer。
     *
     * @param pack_bytes 完整模型包字节（本调用期间必须有效，不会被保留）
     * @param pack_name  仅用于日志/诊断的资产名
     * @note 这里创建的 `Geometry*` 会 `Add` 进 `geo_mgr`，与同步路径一致。
     * @note `material_recipes`/`material_recipe_count` 语义与
     *       `LoadModelAsPrimitiveAssets` 相同（可选多材质表）。
     */
    bool LoadModelFromMemory(
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

    /**
     * @brief 从**已打开的输入流**装载 `.model`（包裹化 / 流化路径的统一入口）。
     *
     * 语义与 `LoadModelFromMemory` 完全一致 —— 内部先把流读完到一块
     * 连续内存，再走同一份字节解析路径。存在的意义是把「资产从哪来」与「怎么解析」
     * 彻底解耦：调用方可以直接喂资产包的 `OpenAssetStream`，不需要物理路径、
     * 不需要先落盘、也不需要自己管缓冲区生命周期。
     *
     * @param stream    已定位到模型包首字节的输入流（**所有权留在调用方**，本函数不关闭）
     * @param pack_name 仅用于日志/诊断的资产名（可以是逻辑路径）
     * @param base_dir  解析几何文件名外置（无 GeometryBlob）时使用的基础目录；
     *                  模型包内嵌 GeometryBlob 时不使用
     *
     * @note 流的读取位置会被推进到结尾；失败不影响流的可用性。
     */
    bool LoadModelFromStream(
        VulkanDevice                     *device,
        GeometryManager                  *geo_mgr,
        const GeometryVertexFormat       &geometry_vertex_format,
        const mtl::MaterialRecipe        *recipe,
        io::InputStream                  *stream,
        const OSString                   &pack_name,
        const OSString                   &base_dir,
        std::vector<PrimitiveAsset>      &out_assets,
        std::vector<StaticMeshNode>      &out_nodes,
        std::vector<int32_t>             &out_root_nodes,
        const mtl::MaterialRecipe *const *material_recipes = nullptr,
        const uint32_t                    material_recipe_count = 0);

    /**
     * @brief 自包含模型目录装配出的材质集（下标 = pack 材质表行号 = material_index）。
     *
     * `table` 指向 `recipes`（本结构拥有配方副本，并已写入
     * `material_ssbo_binding`），可直接喂给上面两个装载函数的
     * `material_recipes`/`material_recipe_count`；`bindings` 可喂给
     * `StaticMeshSceneInstance::Options::material_rows`。
     */
    struct SelfContainedModelMaterials
    {
        std::vector<mtl::MaterialRecipe>        recipes;    ///< 每材质一份副本（含行绑定）
        std::vector<GlobalSSBOBinding>          bindings;   ///< 与 `recipes` 平行
        std::vector<const mtl::MaterialRecipe*> table;      ///< 指向 `recipes`
        const mtl::MaterialRecipe              *fallback = nullptr; ///< = `recipes[0]`
    };

    /**
     * @brief 从**自包含模型目录**装配材质：scene、geometry、recipe、tex2d、
     *        material_data 全部住在 pack 自己的目录里，**严禁跨目录查找**。
     *
     * 流程：
     * 1. 解析 pack 的 V2 MaterialTable（tag 9），取每个材质下标的材质名
     *    （`"<Model>.<idx>.<Name>.material"` → `"<Name>"`）；
     * 2. 只扫 pack 所在目录的 `*.recipe.toml`（经 GraphicsContext 注册表）；
     * 3. 匹配：配方名最后一段 == 材质名；目录里只有一个配方时通吃所有材质
     *    （单材质模型即此形态）；
     * 4. 数据块：目录里唯一一份 `*.material_data.toml`；多份时必须用
     *    `material_data_block_hint` 按文件名（不含扩展名）指定；
     * 5. 按行号 = 材质下标取 `GlobalSSBOBinding`，写进配方副本。
     *
     * @note 仅支持 MDL2 模型包（含 MaterialTable）；缺表的包返回 `false`。
     * @note 成功后 `out` 即材质真源；装载 scene 时把 `out.table` 传入即可，
     *       `out` 需活到 `PrimitiveAsset` 使用完毕（同示例自持配方副本的惯例）。
     */
    bool BuildSelfContainedModelMaterials(
        GraphicsContext             &ctx,
        const OSString              &pack_path,
        const char                  *material_data_block_hint,
        SelfContainedModelMaterials &out);

}//namespace hgl::graph
