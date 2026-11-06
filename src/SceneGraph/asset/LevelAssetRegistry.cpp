#include <hgl/graph/asset/LevelAssetRegistry.h>
#include <hgl/graph/asset/LevelAssetFile.h>
#include <hgl/graph/asset/LevelLoader.h>
#include <hgl/asset/LoadedFile.h>
#include <hgl/log/Log.h>

DEFINE_LOGGER_MODULE(LevelAssetRegistry)
USE_MODULE_LOGGER(LevelAssetRegistry)

namespace hgl::graph
{
    bool LevelAsset::Instantiate(ecs::ECSContext *world,
                                 GeometryCreater *creater,
                                 LevelInstance &out_instance) const
    {
        return LevelLoader::Instantiate(spec, world, creater, out_instance, filename);
    }

    LevelAssetRegistry::~LevelAssetRegistry()
    {
        Clear();
    }

    LevelAsset *LevelAssetRegistry::FindByResourceKey(
        const OSString &resource_key) const
    {
        if (resource_key.IsEmpty())
            return nullptr;

        for (LevelAsset *asset : assets)
            if (asset && asset->resource_key == resource_key)
                return asset;

        return nullptr;
    }

    LevelAsset *LevelAssetRegistry::FindByFilename(const OSString &filename) const
    {
        return FindByResourceKey(MakeLoadedFileKey(filename));
    }

    LevelAsset *LevelAssetRegistry::GetOrLoad(const OSString &filename)
    {
        if (filename.IsEmpty())
        {
            MLogError(LevelAssetRegistry, "GetOrLoad received an empty filename");
            return nullptr;
        }

        const OSString resource_key = MakeLoadedFileKey(filename);
        if (LevelAsset *cached = FindByResourceKey(resource_key))
            return cached;

        LevelAsset *asset = assets.Create();
        if (!asset)
            return nullptr;

        if (!LoadLevelSpecFromFile(filename, asset->spec))
        {
            assets.DeleteAt(assets.GetCount() - 1);
            return nullptr;
        }

        asset->filename = filename;
        asset->resource_key = resource_key;
        return asset;
    }

    void LevelAssetRegistry::Clear()
    {
        assets.Clear();
    }
}
