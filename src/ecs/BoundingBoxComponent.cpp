#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/ECSComponentRecords.h>
#include<array>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
            struct BoundingBoxRecord
            {
                std::array<float, 3> min{};
                std::array<float, 3> max{};
                bool hasWorld = false;
                std::array<float, 3> worldMin{};
                std::array<float, 3> worldMax{};
            };

            std::array<float, 3> ToArray3(const glm::vec3& value)
            {
                return {value.x, value.y, value.z};
            }

            glm::vec3 ToVec3(const std::array<float, 3>& value)
            {
                return glm::vec3(value[0], value[1], value[2]);
            }
        }

        const char* BoundingBoxComponent::GetSerializationType()
        {
            return "BoundingBox";
        }

        bool BoundingBoxComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                     const hgl::UnorderedMap<EntityID, int32_t>&,
                                                     ComponentRecord& out_record)
        {
            auto bbox = std::dynamic_pointer_cast<BoundingBoxComponent>(component);
            if (!bbox)
                return false;

            BoundingBoxRecord data{};
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

            out_record.type = GetSerializationType();
            out_record.payload = data;
            return true;
        }

        void BoundingBoxComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                         Entity* entity,
                                                         std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>&)
        {
            const auto& data = std::any_cast<const BoundingBoxRecord&>(record.payload);
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

        // Static member initialization
        std::shared_ptr<BoundingBoxDataStorage> BoundingBoxComponent::sharedStorage = nullptr;
    }//namespace ecs
}//namespace hgl

