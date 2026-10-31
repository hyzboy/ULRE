#include<hgl/graph/asset/GeometryAssetObject.h>
#include<hgl/asset/generated/GeometryMeta.gen.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        /// 加载器要访问注册器，但 `AssetLoaderFn` 是纯函数指针 ⇒ 只能靠这个
        /// 文件级槽位。约定「主线程、图形初始化后注册一次」，不做并发保护。
        GeometryAssetRegistry *g_geometry_registry = nullptr;

        bool LoadGeometryAsset(const asset::AssetURI &uri,
                               asset::AssetObjectPtr &out,
                               AnsiString &error)
        {
            if (!g_geometry_registry)
            {
                error = "geometry loader: registry not bound";
                return false;
            }

            GeometryAsset *geometry_asset = g_geometry_registry->GetOrLoadURI(uri.raw);

            if (!geometry_asset)
            {
                error  = "geometry loader: resolve failed for ";
                error += asset::AssetURISegmentToAnsi(uri.raw);
                return false;
            }

            out = asset::AssetObjectPtr(new GeometryAssetObject(geometry_asset, uri));

            return true;
        }
    }//namespace

    const asset::ClassMetaInfo *GeometryAssetObject::GetClassMetaInfo() const
    {
        return &asset::generated::Class_GeometryAssetSpec;
    }

    bool RegisterGeometryAssetLoader(GeometryAssetRegistry *registry)
    {
        if (!registry)
        {
            UnregisterGeometryAssetLoader();
            return false;
        }

        g_geometry_registry = registry;

        return asset::AssetFactory::Instance().Register(asset::AssetType::Geometry,
                                                        &LoadGeometryAsset);
    }

    bool UnregisterGeometryAssetLoader()
    {
        g_geometry_registry = nullptr;

        return asset::AssetFactory::Instance().Unregister(asset::AssetType::Geometry);
    }
}//namespace hgl::graph
