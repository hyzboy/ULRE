#include <hgl/graph/asset/MaterialDataBlockObject.h>

#include <hgl/asset/generated/MaterialDataBlockMeta.gen.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

namespace hgl::graph::mtl
{
    namespace
    {
        /// `AssetLoaderFn` 是纯函数指针 ⇒ 只能靠文件级槽位拿到注册表。
        /// 约定「主线程、注册表建好后注册一次」，不做并发保护。
        MaterialDataBlockRegistry *g_material_data_block_registry = nullptr;

        bool LoadMaterialDataBlockAsset(const hgl::asset::AssetURI &uri,
                                        hgl::asset::AssetObjectPtr &out,
                                        AnsiString &error)
        {
            if (!g_material_data_block_registry)
            {
                error = "material data block loader: registry not bound";
                return false;
            }

            MaterialDataBlockAsset *asset =
                g_material_data_block_registry->GetOrLoadURI(uri.raw);

            if (!asset)
            {
                error  = "material data block loader: resolve failed for ";
                error += hgl::asset::AssetURISegmentToAnsi(uri.raw);
                return false;
            }

            if (asset->IsFailed())
            {
                error  = "material data block loader: ";
                error += asset->GetFailureReason();
                return false;
            }

            out = hgl::asset::AssetObjectPtr(new MaterialDataBlockObject(asset, uri));

            return true;
        }
    }//namespace

    const hgl::asset::ClassMetaInfo *MaterialDataBlockObject::GetClassMetaInfo() const
    {
        return &hgl::asset::generated::Class_MaterialDataBlockSpec;
    }

    bool RegisterMaterialDataBlockAssetLoader(MaterialDataBlockRegistry *registry)
    {
        if (!registry)
        {
            UnregisterMaterialDataBlockAssetLoader();
            return false;
        }

        g_material_data_block_registry = registry;

        return hgl::asset::AssetFactory::Instance().Register(
            hgl::asset::AssetType::MaterialData, &LoadMaterialDataBlockAsset);
    }

    bool UnregisterMaterialDataBlockAssetLoader()
    {
        g_material_data_block_registry = nullptr;

        return hgl::asset::AssetFactory::Instance().Unregister(
            hgl::asset::AssetType::MaterialData);
    }
}//namespace hgl::graph::mtl
