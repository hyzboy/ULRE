#pragma once

/**
 * 材质数据块资产的 `AssetObject` 外壳与工厂加载器（`AssetType::MaterialData`）。
 *
 * 为什么需要外壳：`MaterialDataBlockAsset` 本身已经是 `AssetObject` 派生，但它是
 * **注册表拥有**的句柄（表负责析构、上传、区间归还）。`AssetFactory` 吐的是
 * `SharedPtr<AssetObject>`，若直接把句柄塞进去就会出现双重所有权（外壳析构时把
 * 注册表里的对象删掉）。所以这里和 `GeometryAssetObject` 采用同一套做法：
 *
 * ```
 * "mtldata:file:res/.../grid.material_data.toml"
 *        ──AssetFactory──▶ MaterialDataBlockObject  （不拥有）
 *                                │
 *                                └─▶ MaterialDataBlockAsset*  ← 注册表持有
 * ```
 *
 * 外壳只转发身份与规约，不管生命周期；`GetMaterialDataBlockAsset()` 才是要
 * 交给渲染侧的句柄（行区间、首行号都在它身上）。
 */

#include <hgl/asset/AssetFactory.h>
#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>

namespace hgl::graph::mtl
{
    /// `MaterialDataBlockAsset` 的 `AssetObject` 外壳（**不拥有**底层句柄）
    class MaterialDataBlockObject final : public hgl::asset::AssetObject
    {
    private:

        MaterialDataBlockAsset * asset = nullptr;   ///<不拥有（注册表持有）
        hgl::asset::AssetURI     uri;               ///<请求时用的原始定位
        AnsiString               asset_name;

    public:

        MaterialDataBlockObject(MaterialDataBlockAsset *in_asset,
                                const hgl::asset::AssetURI &request_uri)
            : asset(in_asset), uri(request_uri)
        {
            if (asset)
                asset_name = asset->GetAssetName();
        }

        ~MaterialDataBlockObject() override = default;

        const hgl::asset::ClassMetaInfo *GetClassMetaInfo() const override;

        const AnsiString &GetAssetName() const override { return asset_name; }

        /// 底层句柄（不拥有；不要在外部 delete）
        MaterialDataBlockAsset *GetMaterialDataBlockAsset() const { return asset; }

        const hgl::asset::AssetURI &GetURI() const { return uri; }

        /// 已上船才算真能用（`Parsed` 只是数据读进来了）
        bool IsReady() const override { return asset && asset->IsUploaded(); }
    };

    /**
     * 给 `AssetFactory` 装上数据块加载器。
     *
     * `registry` 不拥有，必须比工厂里的注册活得久。传 nullptr 等同
     * `UnregisterMaterialDataBlockAssetLoader()`。
     */
    bool RegisterMaterialDataBlockAssetLoader(MaterialDataBlockRegistry *registry);

    /// 摘掉加载器（之后 `MaterialData` 走 `GenericAssetObject` 降级路径）
    bool UnregisterMaterialDataBlockAssetLoader();
}//namespace hgl::graph::mtl
