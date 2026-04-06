#pragma once

#include <hgl/graph/asset/AssetWorldDef.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hgl::graph
{
    /**
     * AssetWorldRegistry — Application-level registry of static-mesh asset definitions.
     *
     * Lives outside the ECS; ECSContext holds a non-owning pointer to it.
     * Thread-safety: NOT thread-safe — register all assets before spawning entities.
     */
    class AssetWorldRegistry
    {
    public:

        AssetWorldRegistry()  = default;
        ~AssetWorldRegistry() = default;

        // Non-copyable
        AssetWorldRegistry(const AssetWorldRegistry&)            = delete;
        AssetWorldRegistry& operator=(const AssetWorldRegistry&) = delete;

    public:

        /**
         * Register a new asset definition.
         * @param name    Unique human-readable name.
         * @param mesh    Shared ownership of the StaticMesh.
         * @return        Assigned ID (always > 0 on success, INVALID_ID on failure).
         */
        AssetWorldDef::ID Register(const std::string& name, std::shared_ptr<StaticMesh> mesh);

        /**
         * Look up a definition by ID.
         * @return  Pointer to the definition, or nullptr if not found.
         */
        const AssetWorldDef* Get(AssetWorldDef::ID id) const;

        /**
         * Look up a definition by name.
         * @return  Pointer to the definition, or nullptr if not found.
         */
        const AssetWorldDef* GetByName(const std::string& name) const;

        /**
         * Remove an asset definition.  Any live ECS instances referencing it
         * will stop rendering (their AssetInstanceComponent yields null mesh).
         */
        bool Unregister(AssetWorldDef::ID id);

        /// Total number of registered definitions.
        size_t GetCount() const { return id_to_index_.size(); }

        /// Iterate all definitions (read-only).
        const std::vector<AssetWorldDef>& GetAll() const { return defs_; }

    private:

        std::vector<AssetWorldDef>                      defs_;
        std::unordered_map<AssetWorldDef::ID, size_t>   id_to_index_;
        std::unordered_map<std::string, AssetWorldDef::ID> name_to_id_;
        AssetWorldDef::ID                               next_id_ = 1;
    };

}//namespace hgl::graph
