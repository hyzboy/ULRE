#include <hgl/ecs/core/AssetPrimitiveRenderItem.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/TransformComponent.h>
#include <hgl/graph/mesh/Primitive.h>
#include <hgl/vk/VKShaderMaterialProgram.h>
#include <hgl/vk/VKMaterialInstance.h>

namespace hgl::ecs
{
    AssetPrimitiveRenderItem::AssetPrimitiveRenderItem(
        EntityID                            ent_id,
        std::shared_ptr<TransformComponent> trans,
        graph::Primitive*                   prim,
        graph::AssetWorldDef::ID            aid,
        ECSContext*                         ctx)
        : entity_id(ent_id)
        , context(ctx)
        , transform(std::move(trans))
        , primitive(prim)
        , asset_id(aid)
        , world_matrix(1.0f)
    {
        if (transform)
        {
            world_matrix  = transform->GetWorldMatrix();
            worldPosition = transform->GetWorldPosition();
        }
    }

    Entity* AssetPrimitiveRenderItem::GetEntity() const
    {
        if (!context || !entity_id.IsValid())
            return nullptr;
        return context->GetEntity(entity_id);
    }

    graph::MaterialInstance* AssetPrimitiveRenderItem::GetMaterialInstance() const
    {
        return primitive ? primitive->GetMaterialInstance() : nullptr;
    }

    graph::ShaderMaterialProgram* AssetPrimitiveRenderItem::GetMaterial() const
    {
        return primitive ? primitive->GetMaterial() : nullptr;
    }

    void AssetPrimitiveRenderItem::UpdateWorldMatrix()
    {
        if (transform)
        {
            world_matrix  = transform->GetWorldMatrix();
            worldPosition = transform->GetWorldPosition();
        }
    }

}//namespace hgl::ecs
