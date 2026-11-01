#pragma once

/**
 * 几何资产的 `AssetObject` 外壳与工厂加载器。
 *
 * ── 为什么要这一层 ────────────────────────────────────────────────────────────
 * `GeometryAsset` 是 GPU 侧句柄（持 `Geometry*`、有流送状态），不是值语义规约，
 * 因此它**不继承** `AssetObject`。但 `AssetFactory` 统一入口只吐出
 * `AssetObjectPtr`，于是这里提供一层薄适配：
 *
 * ```
 * "geo:builtin:cube"  ──AssetFactory──▶  GeometryAssetObject(AssetObject)
 *                                              │ 不拥有 ↓
 *                                        GeometryAsset*  ← 由 GeometryAssetRegistry 持有
 * ```
 *
 * 适配层**不拥有** `GeometryAsset*`：所有权仍在 `GeometryAssetRegistry`，
 * 外壳只是把「注册器里那个句柄」暴露成一个可放进通用容器的东西。
 * 因此外壳析构不会释放 GPU 资源，句柄生命周期由注册器 `Clear()` 决定。
 *
 * ── 加载器的注册时机 ─────────────────────────────────────────────────────────
 * 几何加载需要设备与注册器，所以在 `GraphicsContext::Initialize()` 之后调用
 * `RegisterGeometryAssetLoader()`。在那之前 `AssetFactory` 对 `AssetType::Geometry`
 * 没有加载器 ⇒ 走 `GenericAssetObject` 降级路径（只记 URI），这正好是离线工具
 * 想要的「不建 GPU 资源也能读元数据」。
 */

#include <hgl/asset/AssetFactory.h>
#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/GeometryAsset.h>
#include <hgl/graph/asset/GeometryAssetRegistry.h>

namespace hgl::graph
{
    /**
     * `GeometryAsset` 的 `AssetObject` 外壳（**不拥有**底层句柄）。
     *
     * `GetClassMetaInfo()` 返回生成物里的 `Class_GeometryAssetSpec` —— 让通用工具
     * 知道「这个对象对应哪种规约结构」，从而能用同一张属性表做检查器/diff。
     */
    class GeometryAssetObject final : public asset::AssetObject
    {
    private:

        GeometryAsset * asset = nullptr;      ///<不拥有（注册器持有）
        asset::AssetURI uri;                  ///<请求时用的原始定位
        AnsiString      asset_name;           ///<规约里的 `name`（可空）

    public:

        GeometryAssetObject(GeometryAsset *geometry_asset,
                            const asset::AssetURI &request_uri)
            : asset(geometry_asset), uri(request_uri)
        {
            if (asset)
                asset_name = asset->GetSpec().name;
        }

        ~GeometryAssetObject() override = default;

        const asset::ClassMetaInfo *GetClassMetaInfo() const override;

        const AnsiString &GetAssetName() const override { return asset_name; }

        /// 底层 GPU 句柄（不拥有；不要在外部 delete）
        GeometryAsset * GetGeometryAsset() const { return asset; }

        /// 该外壳请求时使用的定位 URI
        const asset::AssetURI &GetURI() const { return uri; }

        /// 几何是否已就绪可画（`file:` 来源可能还在流送）
        bool IsReady() const override { return asset && asset->IsReady(); }
    };

    /**
     * 给 `AssetFactory` 装上几何加载器（`AssetType::Geometry`）。
     *
     * `registry` 不拥有，必须比工厂里的注册活得久（正常就是 `GraphicsContext` 的那个）。
     * 传入 nullptr 等同 `UnregisterGeometryAssetLoader()`。
     *
     * 线程约定：与 `GeometryAssetRegistry` 一致，只在主线程调用。
     */
    bool RegisterGeometryAssetLoader(GeometryAssetRegistry *registry);

    /// 摘掉几何加载器（`AssetFactory` 之后对 `Geometry` 走降级路径）
    bool UnregisterGeometryAssetLoader();
}//namespace hgl::graph
