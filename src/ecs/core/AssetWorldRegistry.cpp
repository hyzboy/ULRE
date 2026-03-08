#include <hgl/ecs/core/AssetWorldRegistry.h>

namespace hgl::ecs
{
    bool AssetWorldRegistry::Register(const AssetWorldDef& def)
    {
        if (def.id == 0)
            return false;

        defs[def.id] = def;
        return true;
    }

    bool AssetWorldRegistry::Unregister(AssetWorldId id)
    {
        return defs.erase(id) > 0;
    }

    const AssetWorldDef* AssetWorldRegistry::Get(AssetWorldId id) const
    {
        auto it = defs.find(id);
        if (it == defs.end())
            return nullptr;

        return &it->second;
    }

    bool AssetWorldRegistry::Exists(AssetWorldId id) const
    {
        return defs.find(id) != defs.end();
    }

    void AssetWorldRegistry::Clear()
    {
        defs.clear();
    }

    size_t AssetWorldRegistry::Size() const
    {
        return defs.size();
    }
}
