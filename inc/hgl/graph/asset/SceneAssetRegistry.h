#pragma once

#include <hgl/graph/asset/SceneAssetSpec.h>
#include <hgl/type/ValueArray.h>

namespace hgl::ecs
{
    class ECSContext;
}

namespace hgl::graph
{
    class GeometryCreater;
    class SceneInstance;

    class SceneAsset
    {
        OSString filename;
        OSString resource_key;
        SceneSpec spec;

        friend class SceneAssetRegistry;

    public:
        const OSString &GetFilename() const { return filename; }
        const OSString &GetResourceKey() const { return resource_key; }
        const SceneSpec &GetSpec() const { return spec; }

        bool Instantiate(ecs::ECSContext *world,
                         GeometryCreater *creater,
                         SceneInstance &out_instance) const;
    };

    class SceneAssetRegistry
    {
        ValueArray<SceneAsset *> assets;

        SceneAsset *FindByResourceKey(const OSString &resource_key) const;

    public:
        SceneAssetRegistry() = default;
        ~SceneAssetRegistry();

        SceneAssetRegistry(const SceneAssetRegistry &) = delete;
        SceneAssetRegistry &operator=(const SceneAssetRegistry &) = delete;

        SceneAsset *GetOrLoad(const OSString &filename);
        SceneAsset *FindByFilename(const OSString &filename) const;
        int GetCount() const { return assets.GetCount(); }
        void Clear();
    };
}
