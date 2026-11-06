// LevelAssetRegistry.h — 规格化场景（`LevelSpec`）的句柄表
//
// ── 名字里为什么要带 Spec ────────────────────────────────────────────────────
//
// 引擎里有两种「场景」，彼此不共享任何格式，也不共享加载路径：
//
// | | 本文件的 `LevelAsset` | `ModelAsset`（`ModelAsset.h`） |
// |---|---|---|
// | 数据来源 | 手写/生成的 `LevelSpec`（TOML 或 FlatBuffers） | MDL2 二进制模型包（`.model` MiniPack） |
// | 内容 | 节点名 + 材质参数 + 按名字引用几何/配方 | 顶点/索引/TRS 全在包里，几何可内嵌 |
// | 加载形态 | 同步解析（纯文本/小二进制） | 异步流送（读盘在后台，GPU 在主线程） |
// | 实例化 | `LevelLoader::Instantiate` → `LevelInstance` | `StaticMeshModelInstance` → `PrimitiveAsset` |
//
// 两者都是「场景资产」，但「场景资产」这个名字只能给一个类，因此骨架更轻、
// 只是规格书的那一种让出名字，明确叫 `LevelAsset`。

#pragma once

#include <hgl/graph/asset/LevelSpec.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph
{
    class GeometryCreater;
    class LevelInstance;

    class LevelAsset
    {
        OSString filename;
        OSString resource_key;
        LevelSpec spec;

        friend class LevelAssetRegistry;

    public:
        const OSString &GetFilename() const { return filename; }
        const OSString &GetResourceKey() const { return resource_key; }
        const LevelSpec &GetSpec() const { return spec; }

        bool Instantiate(ecs::ECSContext *world,
                         GeometryCreater *creater,
                         LevelInstance &out_instance) const;
    };

    class LevelAssetRegistry
    {
        ManagedArray<LevelAsset> assets;

        LevelAsset *FindByResourceKey(const OSString &resource_key) const;

    public:
        LevelAssetRegistry() = default;
        ~LevelAssetRegistry();

        LevelAssetRegistry(const LevelAssetRegistry &) = delete;
        LevelAssetRegistry &operator=(const LevelAssetRegistry &) = delete;

        LevelAsset *GetOrLoad(const OSString &filename);
        LevelAsset *FindByFilename(const OSString &filename) const;
        int GetCount() const { return assets.GetCount(); }
        void Clear();
    };
}
