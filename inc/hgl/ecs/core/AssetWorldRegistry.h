#pragma once

#include <hgl/ecs/core/AssetTypes.h>

#include <string>
#include <vector>
#include <unordered_map>

namespace hgl::ecs
{
    struct AssetNodeDef
    {
        AssetNodeId node_id = 0;
        AssetNodeId parent_node_id = 0;
        uint64_t mesh_ref = 0;
        uint64_t material_ref = 0;

        float pos[3] = {0.0f, 0.0f, 0.0f};
        float rot[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float scale[3] = {1.0f, 1.0f, 1.0f};
    };

    struct AssetWorldDef
    {
        AssetWorldId id = 0;
        AssetVersion version = 0;
        std::string name;
        std::vector<AssetNodeDef> nodes;
        float aabb_min[3] = {0.0f, 0.0f, 0.0f};
        float aabb_max[3] = {0.0f, 0.0f, 0.0f};
    };

    class IAssetWorldRegistry
    {
    public:
        virtual ~IAssetWorldRegistry() = default;

        virtual bool Register(const AssetWorldDef& def) = 0;
        virtual bool Unregister(AssetWorldId id) = 0;
        virtual const AssetWorldDef* Get(AssetWorldId id) const = 0;
        virtual bool Exists(AssetWorldId id) const = 0;
    };

    class AssetWorldRegistry final : public IAssetWorldRegistry
    {
    private:
        std::unordered_map<AssetWorldId, AssetWorldDef> defs;

    public:
        bool Register(const AssetWorldDef& def) override;
        bool Unregister(AssetWorldId id) override;
        const AssetWorldDef* Get(AssetWorldId id) const override;
        bool Exists(AssetWorldId id) const override;

        void Clear();
        size_t Size() const;
    };
}
