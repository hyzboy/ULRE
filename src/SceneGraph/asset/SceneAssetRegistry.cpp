#include <hgl/graph/asset/SceneAssetRegistry.h>
#include <hgl/graph/asset/SceneAssetFile.h>
#include <hgl/graph/asset/SceneLoader.h>
#include <hgl/framework/LoadedFile.h>
#include <hgl/log/Log.h>

DEFINE_LOGGER_MODULE(SceneAssetRegistry)
USE_MODULE_LOGGER(SceneAssetRegistry)

namespace hgl::graph
{
    bool SceneAsset::Instantiate(ecs::ECSContext *world,
                                 GeometryCreater *creater,
                                 SceneInstance &out_instance) const
    {
        return SceneLoader::Instantiate(spec, world, creater, out_instance, filename);
    }

    SceneAssetRegistry::~SceneAssetRegistry()
    {
        Clear();
    }

    SceneAsset *SceneAssetRegistry::FindByResourceKey(
        const OSString &resource_key) const
    {
        if (resource_key.IsEmpty())
            return nullptr;

        for (SceneAsset *asset : assets)
            if (asset && asset->resource_key == resource_key)
                return asset;

        return nullptr;
    }

    SceneAsset *SceneAssetRegistry::FindByFilename(const OSString &filename) const
    {
        return FindByResourceKey(MakeLoadedFileKey(filename));
    }

    SceneAsset *SceneAssetRegistry::GetOrLoad(const OSString &filename)
    {
        if (filename.IsEmpty())
        {
            MLogError(SceneAssetRegistry, "GetOrLoad received an empty filename");
            return nullptr;
        }

        const OSString resource_key = MakeLoadedFileKey(filename);
        if (SceneAsset *cached = FindByResourceKey(resource_key))
            return cached;

        SceneAsset *asset = assets.Create();
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

    void SceneAssetRegistry::Clear()
    {
        assets.Clear();
    }
}
