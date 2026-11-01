#include <hgl/graph/asset/EnvironmentAssetObject.h>

#include <hgl/asset/generated/EnvironmentMeta.gen.h>
#include <hgl/log/Log.h>
#include <hgl/utf.h>

namespace hgl::graph
{
    namespace
    {
        EnvironmentAssetRegistry *g_environment_registry = nullptr;

        bool LoadEnvironmentAsset(const hgl::asset::AssetURI &uri,
                                  hgl::asset::AssetObjectPtr &out,
                                  AnsiString &error)
        {
            if (!g_environment_registry)
            {
                error = "environment loader: registry not bound";
                return false;
            }

            EnvironmentAsset *asset =
                g_environment_registry->GetOrLoadURI(uri.raw);

            if (!asset)
            {
                error  = "environment loader: resolve failed for ";
                error += hgl::asset::AssetURISegmentToAnsi(uri.raw);
                if (!g_environment_registry->GetLastError().IsEmpty())
                {
                    error += " (";
                    error += g_environment_registry->GetLastError();
                    error += ")";
                }
                return false;
            }

            out = hgl::asset::AssetObjectPtr(new EnvironmentAssetObject(asset, uri));
            return true;
        }
    }//namespace

    const hgl::asset::ClassMetaInfo *EnvironmentAssetObject::GetClassMetaInfo() const
    {
        return &hgl::asset::generated::Class_EnvironmentSpec;
    }

    bool RegisterEnvironmentAssetLoader(EnvironmentAssetRegistry *registry)
    {
        if (!registry)
        {
            UnregisterEnvironmentAssetLoader();
            return false;
        }

        g_environment_registry = registry;

        return hgl::asset::AssetFactory::Instance().Register(
            hgl::asset::AssetType::Environment, &LoadEnvironmentAsset);
    }

    bool UnregisterEnvironmentAssetLoader()
    {
        g_environment_registry = nullptr;

        return hgl::asset::AssetFactory::Instance().Unregister(
            hgl::asset::AssetType::Environment);
    }
}//namespace hgl::graph
