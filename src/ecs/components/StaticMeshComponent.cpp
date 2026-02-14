#include<hgl/ecs/components/StaticMeshComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

namespace hgl::ecs
{
    void StaticMeshComponent::SetStaticMesh(const std::shared_ptr<StaticMesh>& new_mesh, bool rebuild)
    {
        mesh = new_mesh;

        if (rebuild && auto_instantiate)
            BuildEntities();
    }

    void StaticMeshComponent::SetAutoInstantiate(bool enabled)
    {
        auto_instantiate = enabled;

        if (auto_instantiate && mesh)
            BuildEntities();
    }

    void StaticMeshComponent::OnAttach()
    {
        if (auto_instantiate && mesh)
            BuildEntities();
    }

    void StaticMeshComponent::OnDetach()
    {
        ClearEntities();
    }

    void StaticMeshComponent::BuildEntities()
    {
        if (!owner_context || !mesh)
            return;

        ClearEntities();

        Entity* owner = GetOwner();
        if (!owner)
            return;

        const auto& nodes = mesh->GetNodes();
        const auto& primitives = mesh->GetPrimitives();

        std::vector<EntityID> node_entities;
        const bool has_nodes = !nodes.empty();
        const size_t node_count = has_nodes ? nodes.size() : 1;
        node_entities.resize(node_count);

        for (size_t i = 0; i < node_count; ++i)
        {
            const StaticMesh::Node* node = has_nodes ? &nodes[i] : nullptr;

            std::string node_name;
            if (node && !node->name.empty())
                node_name = node->name;
            else
                node_name = "StaticMeshNode" + std::to_string(i);

            Entity* node_entity = owner_context->CreateEntity<Entity>(node_name);
            if (!node_entity)
                continue;

            auto transform = node_entity->AddComponent<TransformComponent>();
            if (node)
                transform->SetLocalTRS(node->translation, node->rotation, node->scale);
            else
                transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

            node_entities[i] = node_entity->GetID();
            spawned_entities.push_back(node_entity->GetID());
        }

        for (size_t i = 0; i < node_count; ++i)
        {
            const StaticMesh::Node* node = has_nodes ? &nodes[i] : nullptr;

            EntityID parent_id = owner->GetID();
            if (node && node->parent >= 0 && node->parent < static_cast<int>(node_entities.size()))
                parent_id = node_entities[static_cast<size_t>(node->parent)];

            const EntityID node_id = node_entities[i];
            if (!node_id.IsValid())
                continue;

            Entity* node_entity = owner_context->GetEntity(node_id);
            if (!node_entity)
                continue;

            auto transform = node_entity->GetComponent<TransformComponent>();
            if (transform)
                transform->SetParent(parent_id);
        }

        for (size_t i = 0; i < node_count; ++i)
        {
            const StaticMesh::Node* node = has_nodes ? &nodes[i] : nullptr;

            const EntityID node_id = node_entities[i];
            if (!node_id.IsValid())
                continue;

            if (!node)
            {
                for (size_t prim_index = 0; prim_index < primitives.size(); ++prim_index)
                {
                    const auto& prim_info = primitives[prim_index];
                    if (!prim_info.primitive)
                        continue;

                    std::string prim_name = "StaticMeshPrim" + std::to_string(prim_index);
                    Entity* prim_entity = owner_context->CreateEntity<Entity>(prim_name);
                    if (!prim_entity)
                        continue;

                    auto transform = prim_entity->AddComponent<TransformComponent>();
                    transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
                    transform->SetParent(node_id);

                    auto prim_comp = prim_entity->AddComponent<PrimitiveComponent>();
                    prim_comp->SetPrimitive(prim_info.primitive);
                    if (prim_info.material_instance)
                        prim_comp->SetOverrideMaterial(prim_info.material_instance);

                    spawned_entities.push_back(prim_entity->GetID());
                }
                continue;
            }

            for (const auto prim_index : node->primitive_indices)
            {
                if (prim_index >= primitives.size())
                    continue;

                const auto& prim_info = primitives[prim_index];
                if (!prim_info.primitive)
                    continue;

                std::string prim_name = "StaticMeshPrim" + std::to_string(prim_index);
                Entity* prim_entity = owner_context->CreateEntity<Entity>(prim_name);
                if (!prim_entity)
                    continue;

                auto transform = prim_entity->AddComponent<TransformComponent>();
                transform->SetLocalTRS(glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
                transform->SetParent(node_id);

                auto prim_comp = prim_entity->AddComponent<PrimitiveComponent>();
                prim_comp->SetPrimitive(prim_info.primitive);
                if (prim_info.material_instance)
                    prim_comp->SetOverrideMaterial(prim_info.material_instance);

                spawned_entities.push_back(prim_entity->GetID());
            }
        }

        spawned = true;
    }

    void StaticMeshComponent::ClearEntities()
    {
        if (spawned_entities.empty())
        {
            spawned = false;
            return;
        }

        if (owner_context)
        {
            for (const auto& id : spawned_entities)
            {
                if (id.IsValid())
                    owner_context->DestroyEntity(id);
            }
        }

        spawned_entities.clear();
        spawned = false;
    }
}

