#pragma once

#include <hgl/ecs/components/RenderableComponent.h>
#include <hgl/graph/asset/AssetWorldDef.h>

namespace hgl::ecs
{
    /**
     * AssetInstanceComponent
     *
     * Marks an entity as an instance of a registered AssetWorldDef.
     * The AssetInstanceCollectSystem reads this component and emits one
     * AssetPrimitiveRenderItem per Primitive in the referenced StaticMesh.
     *
     * Attach pattern (same as PrimitiveComponent):
     *   auto comp = entity->AddComponent<AssetInstanceComponent>();
     *   comp->SetAssetID(registry.Register("MyMesh", mesh));
     */
    class AssetInstanceComponent : public RenderableComponent
    {
    private:

        hgl::graph::AssetWorldDef::ID asset_id = hgl::graph::AssetWorldDef::INVALID_ID;

    public:

        explicit AssetInstanceComponent(const std::string& name = "AssetInstance")
            : RenderableComponent(name)
        {
        }

        ~AssetInstanceComponent() override = default;

    public:

        // Drives SystemGroup auto-install (mirrors PrimitiveComponent pattern)
        const char* GetSystemGroupName() const override { return "AssetInstance"; }

        void SetAssetID(hgl::graph::AssetWorldDef::ID id) { asset_id = id; }
        hgl::graph::AssetWorldDef::ID GetAssetID() const  { return asset_id; }

        bool IsValid() const { return asset_id != hgl::graph::AssetWorldDef::INVALID_ID; }
    };

}//namespace hgl::ecs
