// AssetLoadGraph.h — 资产装载顺序图（阶段三）
//
// ── 与 `hgl::asset::AssetDependencyGraph` 的分工 ──────────────────────────
//
// CMCore 里已经有一份 `hgl::asset::AssetDependencyGraph`（双向边 + 拓扑序 +
// 引用计数 + RWLock），那份图的键是 `AssetID`，解决的是「身份级」问题：
// 热更波及、安全删除、打包可达闭包。它的前提是**每个资产都先有合法身份**。
//
// 本文件解决的是另一个问题：**这一帧要装这个场景，先把谁准备好、交给哪条
// 流水线**。它必须在「资产还没有身份」的情况下也能工作 —— `res` 里 230 个
// `.Tex2D`、42 个 `.geometry`、46 个 `.material` 都是历史后缀，`DeriveLogical`
// 不认识它们，因此它们不进包裹索引、没有 `AssetID`（`AssetID` 的文档明确
// 规定它是身份而不是内容哈希，不能拿路径哈希去顶）。而它们恰恰是今天真正
// 要装的东西。
//
// 所以这里的身份是「类型 + 去掉类型/编码后缀的包内逻辑路径」：
// `.Tex2D` 与 `tex:` 落到同一个节点，与是否进索引无关。等历史后缀补进
// `DeriveLogical` 之后，只需给 `AssetDepKey` 挂一个可选 `AssetID` 字段，
// 本图即可与身份图对齐，节点结构与算法都不用动。
//
// 本类额外携带两样身份图里没有的东西：
//   · `AssetDepKind` —— 决定派发给 Texture / Geometry / 材质哪条流式系统；
//   · 被引用次数     —— 多实例共享同一份几何/贴图时的去重与卸载判据。

#pragma once

#include <hgl/asset/AssetURI.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <hgl/type/UnorderedMap.h>

#include <cstdint>

namespace hgl::graph
{
    /// 依赖边种类：决定「前置就绪」由哪条流水线负责。
    /// 与 `asset::AssetType` 保持一一对应（另加 `Material` —— MDL2 每材质
    /// 一份二进制，不对应任何 `AssetType`）。
    enum class AssetDepKind : uint8_t
    {
        Geometry = 0,       ///< `.geometry` / `.mesh` / `.geo.*` → GeometryStreamingSystem
        Material,           ///< `.material` 容器（MDL2 每材质一份二进制）
        MaterialRecipe,     ///< `.recipe.toml` 配方
        Texture,            ///< `.Tex2D` / `.TexCube` / `.TexOct` / `texarray:` → TextureStreamingSystem
        MaterialData,       ///< `.material_data.toml` 数据块
        Model,              ///< `.model` 模型包（嵌套模型时作为边出现）
        Primitive,          ///< `.prim` 图元
        Environment,        ///< `.env` 环境配置
    };

    const char *GetAssetDepKindName(AssetDepKind kind);

    /// 包内逻辑路径上限。`res` 里最长的一条（
    /// `ABeautifulGame.StaticMesh/ABeautifulGame.Chessboard_normal`）不到 64 字节，
    /// 这里留足余量。
    enum : int { AssetDepPathMax = 384 };

    /**
     * 规范化的资产身份。
     *
     * 路径内联成定长字符数组而不是 `AnsiString`：`ValueArray` 只接受
     * trivially copyable 类型，而 `ManagedArray` 是「持有指针」的容器，
     * 两者都不适合当值语义的 key。按 `FileInfo` 里 `fullname[HGL_MAX_PATH]`
     * 的做法内联，既满足容器约束，也省掉每个节点的堆分配。
     */
    struct AssetDepKey
    {
        asset::AssetType type = asset::AssetType::Unknown;
        char             path[AssetDepPathMax] = {};

        bool       IsValid() const { return type != asset::AssetType::Unknown && path[0] != 0; }
        AnsiString GetPath() const { return AnsiString(path); }
        AnsiString MakeId() const;

        /// 超长会被截断（不会溢出），返回是否完整写入。
        bool SetPath(const AnsiString &new_path);
    };

    struct AssetDepEdge
    {
        AssetDepKey  target;
        AssetDepKind kind = AssetDepKind::Geometry;
    };

    /// 把模型包/清单里的物理引用规范成资产身份。
    ///
    /// 例：`ABeautifulGame.StaticMesh/ABeautifulGame.Chessboard_normal.Tex2D`
    ///     → { Texture, "abeautifulgame.staticmesh/abeautifulgame.chessboard_normal" }
    ///
    /// `declared_type` 是调用方已知的类型（例如已经在 `tex:` 通道里），用来补上
    /// 后缀推导不出来的场合，并优先于后缀推断。后缀只从**文件名部分**剥，
    /// 目录名里的点不动（`ABeautifulGame.StaticMesh/` 不会被误伤）。
    bool NormalizeAssetRef(const AnsiString &physical_ref,
                           asset::AssetType declared_type,
                           AssetDepKey &out_key);

    inline bool NormalizeAssetRef(const AnsiString &physical_ref, AssetDepKey &out_key)
    {
        return NormalizeAssetRef(physical_ref, asset::AssetType::Unknown, out_key);
    }

    /// 场景资产依赖图：正向边 + 反向引用计数。
    class AssetLoadGraph
    {
    public:
        AssetLoadGraph();
        ~AssetLoadGraph();

        void Clear();

        /// 登记一个节点的全部出边。重复登记会**替换**该节点的旧边，
        /// 所以同一场景重载不会让依赖无限累加。
        /// 返回实际写入的边数（无效目标 / 自环 / 重复目标会被跳过）。
        int SetDependencies(const AssetDepKey &node, const AssetDepEdge *edges, int edge_count);

        int GetDependencyCount(const AssetDepKey &node) const;

        /// 返回节点出边数组，`out_count` 可为 nullptr。节点不存在返回 nullptr。
        const AssetDepEdge *GetDependencies(const AssetDepKey &node, int *out_count = nullptr) const;

        /// 有多少个节点直接引用它。0 = 无引用（可卸载）。
        int GetRefCount(const AssetDepKey &target) const;

        int                GetNodeCount() const { return order_.GetCount(); }
        const AssetDepKey *GetNodeAt(int index) const;

        /// 依赖闭包（**不含**根自身），按「先依赖后使用」的拓扑序排列。
        /// 有环时不死循环：环上节点只出现一次，返回 false 并已尽量填好 out。
        bool SolveClosure(const AssetDepKey &root, ValueArray<AssetDepKey> &out) const;

        /// 诊断用：把「根 → 闭包」打到日志。
        void DumpToLog(const AssetDepKey &root) const;

    private:
        UnorderedMap<AnsiString, ValueArray<AssetDepEdge>> edges_;      // id → 出边
        UnorderedMap<AnsiString, int>                      refs_;       // id → 被引用次数
        UnorderedMap<AnsiString, int>                      node_index_; // id → order_ 下标
        ValueArray<AssetDepKey>                            order_;      // 插入序，稳定遍历/诊断

        int  EnsureNode(const AssetDepKey &key);
        void AddRef(const AnsiString &id, int delta);
    };
}//namespace hgl::graph
