#include<hgl/ecs/Context.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/RenderableComponent.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/CameraComponent.h>

#include<cereal/archives/json.hpp>
#include<cereal/archives/binary.hpp>
#include<cereal/types/array.hpp>
#include<cereal/types/string.hpp>
#include<cereal/types/vector.hpp>
#include<cereal/types/variant.hpp>

#include<array>
#include<cstddef>
#include<fstream>
#include<string>
#include<utility>
#include<unordered_map>
#include<variant>
#include<vector>

namespace hgl::ecs
{
    namespace
    {
        struct TransformData
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

        struct BoundingBoxData
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

        struct RenderableData
        {
            bool visible = true;
            float boundingRadius = 1.0f;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(visible), CEREAL_NVP(boundingRadius));
            }
        };

        struct PrimitiveData
        {
            RenderableData renderable;
            bool hasPrimitive = false;
            bool hasOverrideMaterial = false;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(renderable), CEREAL_NVP(hasPrimitive), CEREAL_NVP(hasOverrideMaterial));
            }
        };

        struct CameraData
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

        using ComponentPayload = std::variant<TransformData, BoundingBoxData, RenderableData, PrimitiveData, CameraData>;

        struct ComponentRecord
        {
            std::string type;
            ComponentPayload payload;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(type), CEREAL_NVP(payload));
            }
        };

        struct EntityRecord
        {
            std::string name;
            std::vector<ComponentRecord> components;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(name), CEREAL_NVP(components));
            }
        };

        struct WorldRecord
        {
            std::vector<EntityRecord> entities;

            template<class Archive>
            void serialize(Archive& ar)
            {
                ar(CEREAL_NVP(entities));
            }
        };

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

        std::array<float, 3> ToArray3(const glm::vec3& value)
        {
            return {value.x, value.y, value.z};
        }

        std::array<float, 4> ToArray4(const glm::quat& value)
        {
            return {value.x, value.y, value.z, value.w};
        }

        glm::vec3 ToVec3(const std::array<float, 3>& value)
        {
            return glm::vec3(value[0], value[1], value[2]);
        }

        glm::quat ToQuat(const std::array<float, 4>& value)
        {
            return glm::quat(value[3], value[0], value[1], value[2]);
        }

        bool SerializeTransform(const std::shared_ptr<Component>& component,
                                const std::unordered_map<EntityID, int32_t>& entity_index,
                                ComponentRecord& out_record)
        {
            auto transform = std::dynamic_pointer_cast<TransformComponent>(component);
            if (!transform)
                return false;

            TransformData data{};
            data.position = ToArray3(transform->GetLocalPosition());
            data.rotation = ToArray4(transform->GetLocalRotation());
            data.scale = ToArray3(transform->GetLocalScale());
            data.movable = transform->IsMovable();

            const auto parent_id = transform->GetParentID();
            auto it = entity_index.find(parent_id);
            if (parent_id.IsValid() && it != entity_index.end())
                data.parentIndex = it->second;

            out_record.type = "Transform";
            out_record.payload = data;
            return true;
        }

        bool SerializeBoundingBox(const std::shared_ptr<Component>& component,
                                  const std::unordered_map<EntityID, int32_t>&,
                                  ComponentRecord& out_record)
        {
            auto bbox = std::dynamic_pointer_cast<BoundingBoxComponent>(component);
            if (!bbox)
                return false;

            BoundingBoxData data{};
            const auto local = bbox->GetAABB();
            data.min = ToArray3(local.GetMin());
            data.max = ToArray3(local.GetMax());
            data.hasWorld = bbox->HasWorldAABB();
            if (data.hasWorld)
            {
                const auto& world = bbox->GetWorldAABB();
                data.worldMin = ToArray3(world.GetMin());
                data.worldMax = ToArray3(world.GetMax());
            }

            out_record.type = "BoundingBox";
            out_record.payload = data;
            return true;
        }

        bool SerializePrimitive(const std::shared_ptr<Component>& component,
                                const std::unordered_map<EntityID, int32_t>&,
                                ComponentRecord& out_record)
        {
            auto primitive = std::dynamic_pointer_cast<PrimitiveComponent>(component);
            if (!primitive)
                return false;

            PrimitiveData data{};
            data.renderable.visible = primitive->IsVisible();
            data.renderable.boundingRadius = primitive->GetBoundingRadius();
            data.hasPrimitive = primitive->GetPrimitive() != nullptr;
            data.hasOverrideMaterial = primitive->GetOverrideMaterial() != nullptr;

            out_record.type = "Primitive";
            out_record.payload = data;
            return true;
        }

        bool SerializeRenderable(const std::shared_ptr<Component>& component,
                                 const std::unordered_map<EntityID, int32_t>&,
                                 ComponentRecord& out_record)
        {
            auto renderable = std::dynamic_pointer_cast<RenderableComponent>(component);
            if (!renderable)
                return false;

            RenderableData data{};
            data.visible = renderable->IsVisible();
            data.boundingRadius = renderable->GetBoundingRadius();

            out_record.type = "Renderable";
            out_record.payload = data;
            return true;
        }

        bool SerializeCamera(const std::shared_ptr<Component>& component,
                             const std::unordered_map<EntityID, int32_t>&,
                             ComponentRecord& out_record)
        {
            auto camera = std::dynamic_pointer_cast<CameraComponent>(component);
            if (!camera)
                return false;

            CameraData data{};
            data.position = ToArray3(camera->position);
            data.target = ToArray3(camera->target);
            data.worldUp = ToArray3(camera->world_up);
            data.fov = camera->fov;
            data.nearPlane = camera->near_plane;
            data.farPlane = camera->far_plane;
            data.yaw = camera->yaw;
            data.pitch = camera->pitch;
            data.roll = camera->roll;
            data.forward = ToArray3(camera->forward);
            data.right = ToArray3(camera->right);
            data.up = ToArray3(camera->up);
            data.controlMode = static_cast<int>(camera->control_mode);
            data.distance = camera->distance;
            data.minDistance = camera->min_distance;
            data.maxDistance = camera->max_distance;
            data.rotationSensitivity = camera->rotation_sensitivity;
            data.zoomSensitivity = camera->zoom_sensitivity;
            data.moveSpeed = camera->move_speed;
            data.inputInvert = {camera->input_invert.x, camera->input_invert.y};
            data.isMainCamera = camera->is_main_camera;
            data.matrixDirty = camera->matrix_dirty;

            out_record.type = "Camera";
            out_record.payload = data;
            return true;
        }

        void DeserializeTransform(const ComponentRecord& record,
                                  Entity* entity,
                                  std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
        {
            const auto& data = std::get<TransformData>(record.payload);
            auto transform = std::make_shared<TransformComponent>();
            transform->SetLocalTRS(ToVec3(data.position), ToQuat(data.rotation), ToVec3(data.scale));
            entity->AddComponentInstance(transform);

            if (data.movable != transform->IsMovable())
                transform->SetMovable(data.movable);

            pending_parents.emplace_back(transform, data.parentIndex);
        }

        void DeserializeBoundingBox(const ComponentRecord& record,
                                    Entity* entity,
                                    std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
        {
            const auto& data = std::get<BoundingBoxData>(record.payload);
            auto bbox = std::make_shared<BoundingBoxComponent>();
            bbox->SetAABB(ToVec3(data.min), ToVec3(data.max));
            if (data.hasWorld)
            {
                hgl::math::AABB world;
                world.SetMinMax(ToVec3(data.worldMin), ToVec3(data.worldMax));
                bbox->SetWorldAABB(world);
            }
            entity->AddComponentInstance(bbox);
        }

        void DeserializePrimitive(const ComponentRecord& record,
                                  Entity* entity,
                                  std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
        {
            const auto& data = std::get<PrimitiveData>(record.payload);
            auto primitive = std::make_shared<PrimitiveComponent>();
            primitive->SetVisible(data.renderable.visible);
            primitive->SetBoundingRadius(data.renderable.boundingRadius);
            entity->AddComponentInstance(primitive);
        }

        void DeserializeRenderable(const ComponentRecord& record,
                                   Entity* entity,
                                   std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
        {
            const auto& data = std::get<RenderableData>(record.payload);
            auto renderable = std::make_shared<RenderableComponent>();
            renderable->SetVisible(data.visible);
            renderable->SetBoundingRadius(data.boundingRadius);
            entity->AddComponentInstance(renderable);
        }

        void DeserializeCamera(const ComponentRecord& record,
                               Entity* entity,
                               std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
        {
            const auto& data = std::get<CameraData>(record.payload);
            auto camera = std::make_shared<CameraComponent>();
            camera->position = ToVec3(data.position);
            camera->target = ToVec3(data.target);
            camera->world_up = ToVec3(data.worldUp);
            camera->fov = data.fov;
            camera->near_plane = data.nearPlane;
            camera->far_plane = data.farPlane;
            camera->yaw = data.yaw;
            camera->pitch = data.pitch;
            camera->roll = data.roll;
            camera->forward = ToVec3(data.forward);
            camera->right = ToVec3(data.right);
            camera->up = ToVec3(data.up);
            camera->control_mode = static_cast<CameraComponent::ControlMode>(data.controlMode);
            camera->distance = data.distance;
            camera->min_distance = data.minDistance;
            camera->max_distance = data.maxDistance;
            camera->rotation_sensitivity = data.rotationSensitivity;
            camera->zoom_sensitivity = data.zoomSensitivity;
            camera->move_speed = data.moveSpeed;
            camera->input_invert = hgl::math::Vector2f(data.inputInvert[0], data.inputInvert[1]);
            camera->is_main_camera = data.isMainCamera;
            camera->matrix_dirty = data.matrixDirty;

            camera->camera_data = nullptr;
            camera->camera_info = nullptr;
            camera->viewport_info = nullptr;
            camera->camera_ubo = nullptr;

            entity->AddComponentInstance(camera);
        }

        const std::vector<ComponentRegistryEntry>& GetComponentRegistry()
        {
            static const std::vector<ComponentRegistryEntry> registry = {
                {"Transform", SerializeTransform, DeserializeTransform},
                {"BoundingBox", SerializeBoundingBox, DeserializeBoundingBox},
                {"Primitive", SerializePrimitive, DeserializePrimitive},
                {"Renderable", SerializeRenderable, DeserializeRenderable},
                {"Camera", SerializeCamera, DeserializeCamera},
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

            return ComponentRecord{"Unknown", RenderableData{}};
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
