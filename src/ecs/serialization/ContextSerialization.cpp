#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/components/SubWorldComponent.h>
#include<hgl/ecs/components/SubSceneMembershipComponent.h>
#include<hgl/ecs/core/ComponentRecords.h>

#include<cereal/archives/json.hpp>
#include<cereal/archives/binary.hpp>
#include<cereal/types/array.hpp>
#include<cereal/types/string.hpp>
#include<cereal/types/vector.hpp>
#include<cereal/types/variant.hpp>

#include<array>
#include<cstddef>
#include<cstdint>
#include<fstream>
#include<string>
#include<utility>
#include <hgl/type/UnorderedMap.h>
#include<variant>
#include<vector>
#include<unordered_set>

namespace hgl::ecs
{
    namespace
    {
        // Serialization record types (internal use only)
        struct TransformRecord
        {
            std::array<float, 3> position{};
            std::array<float, 4> rotation{};
            std::array<float, 3> scale{};
            bool movable = true;
            int32_t parentIndex = -1;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(position), CEREAL_NVP(rotation), CEREAL_NVP(scale),
                   CEREAL_NVP(movable), CEREAL_NVP(parentIndex));
            }
        };

        struct BoundingBoxRecord
        {
            std::array<float, 3> min{};
            std::array<float, 3> max{};
            bool hasWorld = false;
            std::array<float, 3> worldMin{};
            std::array<float, 3> worldMax{};

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(min), CEREAL_NVP(max), CEREAL_NVP(hasWorld),
                   CEREAL_NVP(worldMin), CEREAL_NVP(worldMax));
            }
        };

        struct RenderableRecord
        {
            bool visible = true;
            float boundingRadius = 1.0f;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(visible), CEREAL_NVP(boundingRadius));
            }
        };

        struct PrimitiveRecord
        {
            RenderableRecord renderable;
            bool hasPrimitive = false;
            bool hasOverrideMaterial = false;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(renderable), CEREAL_NVP(hasPrimitive), CEREAL_NVP(hasOverrideMaterial));
            }
        };

        struct CameraRecord
        {
            std::array<float, 3> position{};
            std::array<float, 3> target{};
            std::array<float, 3> worldUp{};

            float fov = 60.0f;
            float nearPlane = 0.1f;
            float farPlane = 1000.0f;

            float yaw = 0.0f;
            float pitch = 0.0f;
            float roll = 0.0f;

            std::array<float, 3> forward{};
            std::array<float, 3> right{};
            std::array<float, 3> up{};

            int controlMode = 0;

            float distance = 0.0f;
            float minDistance = 0.0f;
            float maxDistance = 0.0f;

            float rotationSensitivity = 0.0f;
            float zoomSensitivity = 0.0f;
            float moveSpeed = 0.0f;

            std::array<float, 2> inputInvert{};

            bool isMainCamera = false;
            bool matrixDirty = false;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(position), CEREAL_NVP(target), CEREAL_NVP(worldUp),
                   CEREAL_NVP(fov), CEREAL_NVP(nearPlane), CEREAL_NVP(farPlane),
                   CEREAL_NVP(yaw), CEREAL_NVP(pitch), CEREAL_NVP(roll),
                   CEREAL_NVP(forward), CEREAL_NVP(right), CEREAL_NVP(up),
                   CEREAL_NVP(controlMode), CEREAL_NVP(distance),
                   CEREAL_NVP(minDistance), CEREAL_NVP(maxDistance),
                   CEREAL_NVP(rotationSensitivity), CEREAL_NVP(zoomSensitivity),
                   CEREAL_NVP(moveSpeed), CEREAL_NVP(inputInvert),
                   CEREAL_NVP(isMainCamera), CEREAL_NVP(matrixDirty));
            }
        };

        struct EntityIDRecord
        {
            uint32_t index = UINT32_MAX;
            uint16_t generation = 0;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(index), CEREAL_NVP(generation));
            }
        };

        struct SubWorldRecord
        {
            uint8_t mode = 0;
            bool render_shared = true;
            bool logic_isolated = false;
            uint64_t subscene_id = 0;
            EntityIDRecord root_entity_id{};
            bool paused = false;
            bool tick_enabled = true;
            bool render_enabled = true;
            std::string asset_path;
            bool asset_binary = false;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(mode),
                   CEREAL_NVP(render_shared),
                   CEREAL_NVP(logic_isolated),
                   CEREAL_NVP(subscene_id),
                   CEREAL_NVP(root_entity_id),
                   CEREAL_NVP(paused),
                   CEREAL_NVP(tick_enabled),
                   CEREAL_NVP(render_enabled),
                   CEREAL_NVP(asset_path),
                   CEREAL_NVP(asset_binary));
            }
        };

        struct SubSceneMembershipRecord
        {
            uint64_t subscene_id = 0;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(subscene_id));
            }
        };

        using ComponentPayload = std::variant<TransformRecord, BoundingBoxRecord, RenderableRecord, PrimitiveRecord, CameraRecord, SubWorldRecord, SubSceneMembershipRecord>;

        struct SerializableComponentRecord
        {
            std::string type;
            ComponentPayload payload;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(type), CEREAL_NVP(payload));
            }
        };

        struct SerializableEntityRecord
        {
            std::string name;
            std::vector<SerializableComponentRecord> components;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(name), CEREAL_NVP(components));
            }
        };

        struct SerializableWorldRecord
        {
            std::vector<SerializableEntityRecord> entities;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(entities));
            }
        };

        // Convert ComponentRecord (with std::any) to SerializableComponentRecord (with variant)
        SerializableComponentRecord ToSerializable(const ComponentRecord& record)
        {
            SerializableComponentRecord result;
            result.type = record.type;

            if (record.type == "Transform")
                result.payload = std::any_cast<TransformRecord>(record.payload);
            else if (record.type == "BoundingBox")
                result.payload = std::any_cast<BoundingBoxRecord>(record.payload);
            else if (record.type == "Renderable")
                result.payload = std::any_cast<RenderableRecord>(record.payload);
            else if (record.type == "Primitive")
                result.payload = std::any_cast<PrimitiveRecord>(record.payload);
            else if (record.type == "Camera")
                result.payload = std::any_cast<CameraRecord>(record.payload);
            else if (record.type == "SubWorld")
                result.payload = std::any_cast<SubWorldRecord>(record.payload);
            else if (record.type == "SubSceneMembership")
                result.payload = std::any_cast<SubSceneMembershipRecord>(record.payload);
            else
                result.payload = RenderableRecord{}; // fallback

            return result;
        }

        // Convert SerializableComponentRecord to ComponentRecord
        ComponentRecord FromSerializable(const SerializableComponentRecord& record)
        {
            ComponentRecord result;
            result.type = record.type;
            result.payload = record.payload; // std::any can hold std::variant
            return result;
        }
        using SerializeFn = bool (*)(const std::shared_ptr<Component>&,
                                     const hgl::UnorderedMap<EntityID, int32_t>&,
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
                {SubWorldComponent::GetSerializationType(), SubWorldComponent::SerializeToRecord, SubWorldComponent::DeserializeFromRecord},
                {SubSceneMembershipComponent::GetSerializationType(), SubSceneMembershipComponent::SerializeToRecord, SubSceneMembershipComponent::DeserializeFromRecord},
            };
            return registry;
        }

        const ComponentRegistryEntry* FindRegistryEntry(const std::string& type)
        {
            static const hgl::UnorderedMap<std::string, const ComponentRegistryEntry*> lookup = []()
            {
                hgl::UnorderedMap<std::string, const ComponentRegistryEntry*> table;
                for (const auto& entry : GetComponentRegistry())
                    table.Add(entry.type, &entry);
                return table;
            }();

            auto entry = lookup.GetValuePointer(type);
            if (!entry)
                return nullptr;
            return *entry;
        }

        ComponentRecord BuildComponentRecord(const std::shared_ptr<Component>& component,
                                             const hgl::UnorderedMap<EntityID, int32_t>& entity_index)
        {
            for (const auto& entry : GetComponentRegistry())
            {
                ComponentRecord record;
                if (entry.serialize(component, entity_index, record))
                    return record;
            }

            // Fallback for unknown components
            return ComponentRecord{"Unknown", std::any{}};
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

        bool SaveWorld(const SerializableWorldRecord& world, const std::string& path, bool binary)
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

        bool LoadWorld(SerializableWorldRecord& world, const std::string& path, bool binary)
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

        bool ApplyLoadedWorld(ECSContext* context,
                              const SerializableWorldRecord& world,
                              bool clear_existing,
                              std::vector<EntityID>* out_created_ids)
        {
            if (!context)
                return false;

            if (clear_existing)
                context->ClearEntities();

            std::vector<Entity*> entities;
            entities.reserve(world.entities.size());

            std::vector<EntityID> created_ids;
            created_ids.reserve(world.entities.size());

            for (const auto& record : world.entities)
            {
                Entity* entity = context->CreateEntity<Entity>();
                if (entity)
                {
                    entity->SetName(record.name);
                    created_ids.push_back(entity->GetID());
                }
                entities.push_back(entity);
            }

            std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>> pending_parents;

            for (size_t i = 0; i < world.entities.size(); ++i)
            {
                Entity* entity = entities[i];
                if (!entity)
                    continue;

                const auto& entity_record = world.entities[i];
                for (const auto& serializable_comp : entity_record.components)
                {
                    ComponentRecord comp_record = FromSerializable(serializable_comp);
                    ApplyComponentRecord(comp_record, entity, pending_parents);
                }
            }

            FixupParents(pending_parents, entities);

            if (out_created_ids)
                *out_created_ids = std::move(created_ids);

            return true;
        }
    }

    bool ECSContext::SaveToJson(const std::string& path) const
    {
        std::vector<Entity*> entities;
        if (entity_manager)
            entity_manager->GetAllEntityPointers(entities);

        hgl::UnorderedMap<EntityID, int32_t> entity_index;
        entity_index.Reserve(entities.size());
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (entities[i])
                entity_index[entities[i]->GetID()] = static_cast<int32_t>(i);
        }

        SerializableWorldRecord world;
        world.entities.reserve(entities.size());

        for (auto* entity : entities)
        {
            if (!entity)
                continue;

            SerializableEntityRecord entity_record;
            entity_record.name = entity->GetName();

            std::vector<std::shared_ptr<Component>> components;
            entity->GetAllComponents(components);
            entity_record.components.reserve(components.size());

            for (const auto& component : components)
            {
                if (!component)
                    continue;

                ComponentRecord comp_record = BuildComponentRecord(component, entity_index);
                entity_record.components.push_back(ToSerializable(comp_record));
            }

            world.entities.push_back(std::move(entity_record));
        }

        return SaveWorld(world, path, false);
    }

    bool ECSContext::LoadFromJson(const std::string& path)
    {
        SerializableWorldRecord world;
        if (!LoadWorld(world, path, false))
            return false;

        if (!ApplyLoadedWorld(this, world, true, nullptr))
            return false;

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

    bool ECSContext::ImportFromJson(const std::string& path, std::vector<EntityID>* out_created_ids)
    {
        SerializableWorldRecord world;
        if (!LoadWorld(world, path, false))
            return false;

        if (!ApplyLoadedWorld(this, world, false, out_created_ids))
            return false;

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

        hgl::UnorderedMap<EntityID, int32_t> entity_index;
        entity_index.Reserve(entities.size());
        for (size_t i = 0; i < entities.size(); ++i)
        {
            if (entities[i])
                entity_index[entities[i]->GetID()] = static_cast<int32_t>(i);
        }

        SerializableWorldRecord world;
        world.entities.reserve(entities.size());

        for (auto* entity : entities)
        {
            if (!entity)
                continue;

            SerializableEntityRecord entity_record;
            entity_record.name = entity->GetName();

            std::vector<std::shared_ptr<Component>> components;
            entity->GetAllComponents(components);
            entity_record.components.reserve(components.size());

            for (const auto& component : components)
            {
                if (!component)
                    continue;

                ComponentRecord comp_record = BuildComponentRecord(component, entity_index);
                entity_record.components.push_back(ToSerializable(comp_record));
            }

            world.entities.push_back(std::move(entity_record));
        }

        return SaveWorld(world, path, true);
    }

    bool ECSContext::LoadFromBinary(const std::string& path)
    {
        SerializableWorldRecord world;
        if (!LoadWorld(world, path, true))
            return false;

        if (!ApplyLoadedWorld(this, world, true, nullptr))
            return false;

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

    bool ECSContext::ImportFromBinary(const std::string& path, std::vector<EntityID>* out_created_ids)
    {
        SerializableWorldRecord world;
        if (!LoadWorld(world, path, true))
            return false;

        if (!ApplyLoadedWorld(this, world, false, out_created_ids))
            return false;

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

    bool ECSContext::InstantiateAssetAsChildren(const std::string& path,
                                                EntityID parent_id,
                                                bool binary,
                                                AssetInstance* out_instance)
    {
        if (!parent_id.IsValid())
            return false;

        Entity* parent_entity = GetEntity(parent_id);
        if (!parent_entity)
            return false;

        std::vector<EntityID> created_ids;
        const bool ok = binary
                      ? ImportFromBinary(path, &created_ids)
                      : ImportFromJson(path, &created_ids);

        if (!ok)
            return false;

        std::unordered_set<EntityID> imported_set;
        imported_set.reserve(created_ids.size());
        for (const auto& id : created_ids)
        {
            if (id.IsValid())
                imported_set.insert(id);
        }

        for (const auto& id : created_ids)
        {
            if (!id.IsValid())
                continue;

            Entity* entity = GetEntity(id);
            if (!entity)
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                continue;

            const EntityID current_parent = transform->GetParentID();
            if (!current_parent.IsValid() || imported_set.find(current_parent) == imported_set.end())
                transform->SetParent(parent_id);
        }

        if (out_instance)
            out_instance->entity_ids = std::move(created_ids);

        return true;
    }

    void ECSContext::DestroyAssetInstance(const AssetInstance& instance)
    {
        for (const auto& id : instance.entity_ids)
        {
            if (id.IsValid())
                DestroyEntity(id);
        }
    }
}


