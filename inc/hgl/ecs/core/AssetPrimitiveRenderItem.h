#pragma once

#include <hgl/ecs/core/RenderItem.h>
#include <hgl/graph/asset/AssetWorldDef.h>
#include <glm/glm.hpp>
#include <memory>

namespace hgl
{
    namespace graph
    {
        class Primitive;
        class MaterialTemplate;
        class MaterialInstance;
    }
}

namespace hgl::ecs
{
    class Entity;
    class ECSContext;
    class TransformComponent;
    class RenderableComponent;

    /**
     * AssetPrimitiveRenderItem
     *
     * A RenderItem backed by a raw graph::Primitive* taken directly from an
     * AssetWorldDef's StaticMesh.  Unlike PrimitiveRenderItem (which wraps a
     * PrimitiveComponent), this item is created by AssetInstanceCollectSystem
     * — one item per Primitive per entity — and does not require a dedicated
     * ECS component per primitive.
     */
    class AssetPrimitiveRenderItem : public RenderItem
    {
    private:

        EntityID                            entity_id;
        ECSContext*                         context    = nullptr;
        std::shared_ptr<TransformComponent> transform;
        graph::Primitive*                   primitive  = nullptr;   ///< Non-owning; owned by StaticMesh
        graph::AssetWorldDef::ID            asset_id   = graph::AssetWorldDef::INVALID_ID;
        glm::mat4                           world_matrix{1.0f};

    public:

        AssetPrimitiveRenderItem(EntityID ent_id,
                                 std::shared_ptr<TransformComponent> trans,
                                 graph::Primitive* prim,
                                 graph::AssetWorldDef::ID aid,
                                 ECSContext* ctx = nullptr);

        ~AssetPrimitiveRenderItem() override = default;

    public:

        // ---- RenderItem abstract interface ----

        EntityID                            GetEntityID()    const override { return entity_id; }
        Entity*                             GetEntity()      const override;
        std::shared_ptr<TransformComponent> GetTransform()   const override { return transform; }
        std::shared_ptr<RenderableComponent> GetRenderable() const override { return nullptr; }
        glm::mat4                           GetWorldMatrix() const override { return world_matrix; }

        graph::Primitive*        GetPrimitive()        const override { return primitive; }
        graph::MaterialInstance* GetMaterialInstance() const override;
        graph::MaterialTemplate*         GetMaterial()         const override;

        // ---- Asset-specific ----

        graph::AssetWorldDef::ID GetAssetID() const { return asset_id; }
        graph::Primitive*        GetPrimitivePtr() const { return primitive; }

        /// Recompute world_matrix from the current TransformComponent state.
        void UpdateWorldMatrix();
    };

}//namespace hgl::ecs
