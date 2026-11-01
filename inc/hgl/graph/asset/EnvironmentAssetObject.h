#pragma once

/**
 * 环境资产的 `AssetObject` 外壳与工厂加载器（`AssetType::Environment`）。
 *
 * 遵循与 `GeometryAssetObject`、`MaterialDataBlockObject` 完全一致的轻量适配设计：
 *
 * ```
 * "env:file:res/.../sky.env"
 *        ──AssetFactory──▶ EnvironmentAssetObject  （不拥有）
 *                                │
 *                                └─▶ EnvironmentAsset*  ← 注册表持有
 * ```
 *
 * 外壳只转发身份与元数据，不管生命周期。
 */

#include <hgl/asset/AssetFactory.h>
#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/EnvironmentAsset.h>
#include <hgl/graph/asset/EnvironmentAssetRegistry.h>

namespace hgl::graph
{
    /// `EnvironmentAsset` 的 `AssetObject` 外壳（不拥有底层句柄）
    class EnvironmentAssetObject final : public hgl::asset::AssetObject
    {
    private:

        EnvironmentAsset *      asset = nullptr;   ///<不拥有（注册表持有）
        hgl::asset::AssetURI    uri;               ///<请求时用的原始定位
        AnsiString              asset_name;

    public:

        EnvironmentAssetObject(EnvironmentAsset *in_asset,
                               const hgl::asset::AssetURI &request_uri)
            : asset(in_asset), uri(request_uri)
        {
            if (asset)
                asset_name = asset->GetEnvironmentName();
        }

        ~EnvironmentAssetObject() override = default;

        const hgl::asset::ClassMetaInfo *GetClassMetaInfo() const override;

        const AnsiString &GetAssetName() const override { return asset_name; }

        bool Serialize(hgl::serialize::Archive &ar) override { return asset ? asset->Serialize(ar) : false; }

        /// 底层句柄（不拥有；不要在外部 delete）
        EnvironmentAsset *GetEnvironmentAsset() const { return asset; }

        const hgl::asset::AssetURI &GetURI() const { return uri; }
    };

    /**
     * 给 `AssetFactory` 装上环境配置加载器。
     *
     * `registry` 不拥有，必须比工厂里的注册活得久。传 nullptr 等同
     * `UnregisterEnvironmentAssetLoader()`。
     */
    bool RegisterEnvironmentAssetLoader(EnvironmentAssetRegistry *registry);

    /// 摘掉加载器（之后 `Environment` 走 `GenericAssetObject` 降级路径）
    bool UnregisterEnvironmentAssetLoader();
}//namespace hgl::graph
