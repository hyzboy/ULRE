#include <hgl/graph/asset/SceneSpecAssetRegistry.h>
#include <hgl/graph/asset/SceneAssetFile.h>
#include <hgl/graph/asset/SceneLoader.h>
#include <hgl/asset/LoadedFile.h>
#include <hgl/log/Log.h>

DEFINE_LOGGER_MODULE(SceneSpecAssetRegistry)
USE_MODULE_LOGGER(SceneSpecAssetRegistry)

namespace hgl::graph
{
    bool SceneSpecAsset::Instantiate(ecs::ECSContext *world,
                                 GeometryCreater *creater,
                                 SceneInstance &out_instance) const
    {
        return SceneLoader::Instantiate(spec, world, creater, out_instance, filename);
    }

    SceneSpecAssetRegistry::~SceneSpecAssetRegistry()
    {
        Clear();
    }

    SceneSpecAsset *SceneSpecAssetRegistry::FindByResourceKey(
        const OSString &resource_key) const
    {
        if (resource_key.IsEmpty())
            return nullptr;

        for (SceneSpecAsset *asset : assets)
            if (asset && asset->resource_key == resource_key)
                return asset;

        return nullptr;
    }

    SceneSpecAsset *SceneSpecAssetRegistry::FindByFilename(const OSString &filename) const
    {
        return FindByResourceKey(MakeLoadedFileKey(filename));
    }

    SceneSpecAsset *SceneSpecAssetRegistry::GetOrLoad(const OSString &filename)
    {
        if (filename.IsEmpty())
        {
            MLogError(SceneSpecAssetRegistry, "GetOrLoad received an empty filename");
            return nullptr;
        }

        const OSString resource_key = MakeLoadedFileKey(filename);
        if (SceneSpecAsset *cached = FindByResourceKey(resource_key))
            return cached;

        SceneSpecAsset *asset = assets.Create();
        if (!asset)
            return nullptr;

        if (!LoadSceneSpecFromFile(filename, asset->spec))
        {
            assets.DeleteAt(assets.GetCount() - 1);
            return nullptr;
        }

        asset->filename = filename;
        asset->resource_key = resource_key;
        return asset;
    }

    void SceneSpecAssetRegistry::Clear()
    {
        assets.Clear();
    }
}
