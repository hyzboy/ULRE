// ModelDependencyExtractor.h — 从模型包 / 材质配方里抽出依赖，喂给 AssetLoadGraph
//
// 阶段三的关键约束：**不装载也能知道要装什么**。
// 因此这里的两个函数都只读原始字节 —— 不碰 `GraphicsContext`、不建 GPU 资源、
// 不解析几何顶点，只把「引用」翻成规范化身份后写进图里。
//
// 依赖来源只有两处（MDL2 把几何顶点内嵌在 GeometryBlob 里，所以几何通常不是
// 外部依赖；只有早期 V2 包会回退到外部 `.geometry` 文件）：
//   1. `MaterialTable` 每行的 StringPool 串（形如 `<Scene>.<idx>.<Name>.material`）
//   2. `PrimitiveTable` 在**没有** GeometryBlob/GeometryView 时的几何文件名
// 纹理依赖不在模型包里，而在各 `*.recipe.toml` 的 `[[textures]] resource_id`，
// 由 `ExtractRecipeDependencies` 处理。

#pragma once

#include <hgl/graph/asset/AssetLoadGraph.h>
#include <hgl/type/String.h>
#include <hgl/type/ManagedArray.h>

#include <cstdint>

namespace hgl::graph
{
    /// 把包内相对名接到基准目录上。`base_dir` 为空时原样返回；
    /// 否则保证中间只有一个 `/`（`base_dir` 末尾带不带分隔符都可以）。
    AnsiString JoinAssetRef(const AnsiString &base_dir, const AnsiString &name);

    /**
     * 模型包里的一条材质引用。
     *
     * `name` 是**给人看的材质名**（`Chessboard`），`key` 是**给图用的身份**
     * （`material_recipe:abeautifulgame.staticmesh/abeautifulgame.0.chessboard`）。
     * 两者都要：配方文件名按名字匹配，依赖边按身份连接。
     */
    struct ModelMaterialEntry
    {
        AnsiString  name;       ///<已剥 `.material` 后缀与 `<Scene>.<idx>.` 前缀
        AssetDepKey key;        ///<规范化后的材质节点身份
    };

    /**
     * 从已取到的 MDL2 包头 + 载荷中抽出场景的外部依赖。
     *
     * @param pack_header_raw `ModelPackHeader` 条目首地址
     * @param header_len      该条目字节数
     * @param payload_raw     `ModelPayload` 条目首地址
     * @param payload_len     该条目字节数
     * @param base_dir        包内相对目录（`res` 包裹里即 `ABeautifulGame.StaticMesh/`）
     * @param scene_ref       场景自身的引用（用于规范化出根节点）
     * @param graph           依赖写进这张图（整体替换 `scene_ref` 的出边）
     * @param out_edge_count  可为 nullptr；返回实际写入的边数
     * @param out_materials   可为 nullptr；非空时写入包内材质表（名字 + 身份），
     *                        供上层按名字匹配同目录的 `*.recipe.toml`
     * @param out_embedded_geometry 可为 nullptr；非空时返回**内嵌**在包里的几何
     *                        份数。自足包不产生外部几何依赖，因此「几何=0」只是
     *                        「不需要为几何做外部 IO」，并不等于场景里没有几何。
     *
     * @return false = 包结构不合法。此时**不要**把「零依赖」当成结论。
     */
    bool ExtractModelDependencies(const void *pack_header_raw, uint32_t header_len,
                                  const void *payload_raw,    uint32_t payload_len,
                                  const AnsiString &base_dir,
                                  const AnsiString &scene_ref,
                                  AssetLoadGraph &graph,
                                  int *out_edge_count = nullptr,
                                  ManagedArray<ModelMaterialEntry> *out_materials = nullptr,
                                  int *out_embedded_geometry = nullptr);

    /**
     * 从配方文本中抽出纹理依赖（`[[textures]]` 的 `resource_id`）。
     *
     * 只登记 `resource_id` 明确给出的项：配方级 `TextureAsset*` 句柄是运行期对象，
     * 不在文本里出现。纹理名（`texture_name`）是语义键而不是资源位置，不能当依赖。
     *
     * @return false = 配方解析失败（键值错误）。缺 `resource_id` 不算失败。
     */
    bool ExtractRecipeDependencies(const char *content, int content_size,
                                   const AnsiString &base_dir,
                                   const AnsiString &recipe_ref,
                                   AssetLoadGraph &graph,
                                   int *out_edge_count = nullptr);
}//namespace hgl::graph
