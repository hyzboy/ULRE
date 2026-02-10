#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/RenderableComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/CameraComponent.h>
#include<hgl/ecs/ECSComponentRecords.h>

#include<cereal/archives/json.hpp>
#include<cereal/archives/binary.hpp>

#include<cstddef>
#include<fstream>
#include<string>
#include<utility>
#include<unordered_map>
#include<vector>

namespace hgl::ecs
{
    namespace
    {
        using SerializeFn = bool (*)(const std::shared_ptr<Component>&,
                                     const std::unordered_map<EntityID, int32_t>&,
                                     ComponentRecord&);
        using DeserializeFn = void (*)(const ComponentRecord&,
                                       Entity*,
                                       std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&);

        struct ComponentRegistryEntry
        {
            const char* type;
            SerializeFn serialize;
            DeserializeFn deserialize;
        };

        const std::vector<ComponentRegistryEntry>& GetComponentRegistry()
        {
            static const std::vector<ComponentRegistryEntry> registry = {
                {TransformComponent::GetSerializationType(), TransformComponent::SerializeToRecord, TransformComponent::DeserializeFromRecord},
                {BoundingBoxComponent::GetSerializationType(), BoundingBoxComponent::SerializeToRecord, BoundingBoxComponent::DeserializeFromRecord},
                {PrimitiveComponent::GetSerializationType(), PrimitiveComponent::SerializeToRecord, PrimitiveComponent::DeserializeFromRecord},
                {RenderableComponent::GetSerializationType(), RenderableComponent::SerializeToRecord, RenderableComponent::DeserializeFromRecord},
                {CameraComponent::GetSerializationType(), CameraComponent::SerializeToRecord, CameraComponent::DeserializeFromRecord},
            };
            return registry;
        }

        const ComponentRegistryEntry* FindRegistryEntry(const std::string& type)
        {
            static const std::unordered_map<std::string, const ComponentRegistryEntry*> lookup = []()
            {
                std::unordered_map<std::string, const ComponentRegistryEntry*> table;
                for (const auto& entry : GetComponentRegistry())
                    table.emplace(entry.type, &entry);
                return table;
            }();

            auto it = lookup.find(type);
            if (it == lookup.end())
                return nullptr;
            return it->second;
        }

        ComponentRecord BuildComponentRecord(const std::shared_ptr<Component>& component,
                                             const std::unordered_map<EntityID, int32_t>& entity_index)
        {
            for (const auto& entry : GetComponentRegistry())
            {
                ComponentRecord record;
                if (entry.serialize(component, entity_index, record))
                    return record;
            }

            // Fallback for unknown components
            return ComponentRecord{"Unknown", RenderableRecord{}};
        }

        void ApplyComponentRecord(const ComponentRecord& record,
                                  Entity* entity,
                                  std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
        {
            if (!entity)
                return;

            const auto* entry = FindRegistryEntry(record.type);
            if (!entry || !entry->deserialize)
                return;

            entry->deserialize(record, entity, pending_parents);
        }

        void FixupParents(const std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents,
                          const std::vector<Entity*>& entities)
        {
            for (const auto& pair : pending_parents)
            {
                const auto& transform = pair.first;
                const int32_t parent_index = pair.second;

                if (!transform)
                    continue;

                if (parent_index < 0 || static_cast<size_t>(parent_index) >= entities.size())
                    continue;

                Entity* parent_entity = entities[static_cast<size_t>(parent_index)];
                if (!parent_entity)
                    continue;

                transform->SetParent(parent_entity->GetID());
            }
        }

        bool SaveWorld(const WorldRecord& world, const std::string& path, bool binary)
        {
            std::ofstream stream(path, binary ? std::ios::binary : std::ios::out);
            if (!stream.is_open())
                return false;

            if (binary)
            {
                cereal::BinaryOutputArchive archive(stream);
                archive(cereal::make_nvp("world", world));
            }
            else
            {
                cereal::JSONOutputArchive archive(stream);
                archive(cereal::make_nvp("world", world));
            }

            return true;
        }

        bool LoadWorld(WorldRecord& world, const std::string& path, bool binary)
        {
            std::ifstream stream(path, binary ? std::ios::binary : std::ios::in);
            if (!stream.is_open())
                return false;

            if (binary)
            {
                cereal::BinaryInputArchive archive(stream);
                archive(cereal::make_nvp("world", world));
            }
            else
            {
                cereal::JSONInputArchive archive(stream);
                archive(cereal::make_nvp("world", world));
            }

            return true;
        }
    }

    bool ECSContext::SaveToJson(const std::string& path) const
    {
        std::vector<Entity*> entities;
        if (entity_manager)
            entity_manager->GetAllEntityPointers(entities);

        std::unordered_map<EntityID, int32_t> entity_index;
        entity_index.reserve(entities.size());
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (entities[i])
                entity_index[entities[i]->GetID()] = static_cast<int32_t>(i);
        }

        WorldRecord world;
        world.entities.reserve(entities.size());

        for (auto* entity : entities)
        {
            if (!entity)
                continue;

            EntityRecord record;
            record.name = entity->GetName();

            std::vector<std::shared_ptr<Component>> components;
            entity->GetAllComponents(components);
            record.components.reserve(components.size());

            for (const auto& component : components)
            {
                if (!component)
                    continue;

                record.components.push_back(BuildComponentRecord(component, entity_index));
            }

            world.entities.push_back(std::move(record));
        }

        return SaveWorld(world, path, false);
    }

    bool ECSContext::LoadFromJson(const std::string& path)
    {
        WorldRecord world;
        if (!LoadWorld(world, path, false))
            return false;

        ClearEntities();

        std::vector<Entity*> entities;
        entities.reserve(world.entities.size());

        for (const auto& record : world.entities)
        {
            Entity* entity = CreateEntity<Entity>();
            if (entity)
                entity->SetName(record.name);
            entities.push_back(entity);
        }

        std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>> pending_parents;

        for (size_t i = 0; i < world.entities.size(); ++i)
        {
            Entity* entity = entities[i];
            if (!entity)
                continue;

            const auto& record = world.entities[i];
            for (const auto& component_record : record.components)
            {
                ApplyComponentRecord(component_record, entity, pending_parents);
            }
        }

        FixupParents(pending_parents, entities);

        for (auto& entry : tick_systems)
        {
            if (entry.second && entry.second->GetCache())
                entry.second->GetCache()->RebuildAll();
        }

        for (auto& entry : render_systems)
        {
            if (entry.second && entry.second->GetCache())
                entry.second->GetCache()->RebuildAll();
        }

        return true;
    }

    bool ECSContext::SaveToBinary(const std::string& path) const
    {
        std::vector<Entity*> entities;
        if (entity_manager)
            entity_manager->GetAllEntityPointers(entities);

        std::unordered_map<EntityID, int32_t> entity_index;
        entity_index.reserve(entities.size());
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (entities[i])
                entity_index[entities[i]->GetID()] = static_cast<int32_t>(i);
        }

        WorldRecord world;
        world.entities.reserve(entities.size());

        for (auto* entity : entities)
        {
            if (!entity)
                continue;

            EntityRecord record;
            record.name = entity->GetName();

            std::vector<std::shared_ptr<Component>> components;
            entity->GetAllComponents(components);
            record.components.reserve(components.size());

            for (const auto& component : components)
            {
                if (!component)
                    continue;

                record.components.push_back(BuildComponentRecord(component, entity_index));
            }

            world.entities.push_back(std::move(record));
        }

        return SaveWorld(world, path, true);
    }

    bool ECSContext::LoadFromBinary(const std::string& path)
    {
        WorldRecord world;
        if (!LoadWorld(world, path, true))
            return false;

        ClearEntities();

        std::vector<Entity*> entities;
        entities.reserve(world.entities.size());

        for (const auto& record : world.entities)
        {
            Entity* entity = CreateEntity<Entity>();
            if (entity)
                entity->SetName(record.name);
            entities.push_back(entity);
        }

        std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>> pending_parents;

        for (size_t i = 0; i < world.entities.size(); ++i)
        {
            Entity* entity = entities[i];
            if (!entity)
                continue;

            const auto& record = world.entities[i];
            for (const auto& component_record : record.components)
            {
                ApplyComponentRecord(component_record, entity, pending_parents);
            }
        }

        FixupParents(pending_parents, entities);

        for (auto& entry : tick_systems)
        {
            if (entry.second && entry.second->GetCache())
                entry.second->GetCache()->RebuildAll();
        }

        for (auto& entry : render_systems)
        {
            if (entry.second && entry.second->GetCache())
                entry.second->GetCache()->RebuildAll();
        }

        return true;
    }
}
