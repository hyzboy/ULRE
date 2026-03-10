#include <hgl/graph/asset/AssetWorldRegistry.h>
#include <algorithm>

namespace hgl::graph
{
    AssetWorldDef::ID AssetWorldRegistry::Register(const std::string& name,
                                                   std::shared_ptr<StaticMesh> mesh)
    {
        if (name.empty() || !mesh)
            return AssetWorldDef::INVALID_ID;

        // Reject duplicate names
        if (name_to_id_.count(name))
            return AssetWorldDef::INVALID_ID;

        const AssetWorldDef::ID new_id = next_id_++;

        const size_t index = defs_.size();
        defs_.emplace_back(new_id, name, std::move(mesh));
        id_to_index_[new_id] = index;
        name_to_id_[name]    = new_id;

        return new_id;
    }

    const AssetWorldDef* AssetWorldRegistry::Get(AssetWorldDef::ID id) const
    {
        auto it = id_to_index_.find(id);
        if (it == id_to_index_.end())
            return nullptr;
        return &defs_[it->second];
    }

    const AssetWorldDef* AssetWorldRegistry::GetByName(const std::string& name) const
    {
        auto it = name_to_id_.find(name);
        if (it == name_to_id_.end())
            return nullptr;
        return Get(it->second);
    }

    bool AssetWorldRegistry::Unregister(AssetWorldDef::ID id)
    {
        auto it = id_to_index_.find(id);
        if (it == id_to_index_.end())
            return false;

        const size_t index = it->second;
        const std::string& name = defs_[index].name;

        name_to_id_.erase(name);
        id_to_index_.erase(it);

        // Swap-erase from defs_ and update the displaced entry's index map
        if (index != defs_.size() - 1)
        {
            std::swap(defs_[index], defs_.back());
            id_to_index_[defs_[index].id] = index;
        }
        defs_.pop_back();

        return true;
    }

}//namespace hgl::graph
