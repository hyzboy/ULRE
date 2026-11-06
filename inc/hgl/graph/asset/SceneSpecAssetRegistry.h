// SceneSpecAssetRegistry.h — 规格化场景（`SceneSpec`）的句柄表
//
// ── 名字里为什么要带 Spec ────────────────────────────────────────────────────
//
// 引擎里有两种「场景」，彼此不共享任何格式，也不共享加载路径：
//
// | | 本文件的 `SceneSpecAsset` | `SceneAsset`（`SceneAsset.h`） |
// |---|---|---|
// | 数据来源 | 手写/生成的 `SceneSpec`（TOML 或 FlatBuffers） | SCN2 二进制场景包（`.scene` MiniPack） |
// | 内容 | 节点名 + 材质参数 + 按名字引用几何/配方 | 顶点/索引/TRS 全在包里，几何可内嵌 |
// | 加载形态 | 同步解析（纯文本/小二进制） | 异步流送（读盘在后台，GPU 在主线程） |
// | 实例化 | `SceneLoader::Instantiate` → `SceneInstance` | `StaticMeshSceneInstance` → `PrimitiveAsset` |
//
// 两者都是「场景资产」，但「场景资产」这个名字只能给一个类，因此骨架更轻、
// 只是规格书的那一种让出名字，明确叫 `SceneSpecAsset`。

#pragma once

#include <hgl/graph/asset/SceneAssetSpec.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph
{
    class GeometryCreater;
    class SceneInstance;

    class SceneSpecAsset
    {
        OSString filename;
        OSString resource_key;
        SceneSpec spec;

        friend class SceneSpecAssetRegistry;

    public:
        const OSString &GetFilename() const { return filename; }
        const OSString &GetResourceKey() const { return resource_key; }
        const SceneSpec &GetSpec() const { return spec; }

        bool Instantiate(ecs::ECSContext *world,
                         GeometryCreater *creater,
                         SceneInstance &out_instance) const;
    };

    class SceneSpecAssetRegistry
    {
        ManagedArray<SceneSpecAsset> assets;

        SceneSpecAsset *FindByResourceKey(const OSString &resource_key) const;

    public:
        SceneSpecAssetRegistry() = default;
        ~SceneSpecAssetRegistry();

        SceneSpecAssetRegistry(const SceneSpecAssetRegistry &) = delete;
        SceneSpecAssetRegistry &operator=(const SceneSpecAssetRegistry &) = delete;

        SceneSpecAsset *GetOrLoad(const OSString &filename);
        SceneSpecAsset *FindByFilename(const OSString &filename) const;
        int GetCount() const { return assets.GetCount(); }
        void Clear();
    };
}
