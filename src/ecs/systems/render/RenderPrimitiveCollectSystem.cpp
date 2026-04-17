#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>
#include<cstdio>

namespace hgl::ecs
{
    namespace
    {
        constexpr const char *kTrackedWireMaterialId = "bounds_wire";

        static bool IsTrackedWire(const PrimitiveComponent *comp)
        {
            if (!comp)
                return false;

            const auto &slot = comp->GetMaterialSlot();
            return slot.record
                && !slot.record->id.empty()
                && slot.record->id == kTrackedWireMaterialId;
        }
    }

    RenderPrimitiveCollectSystem::RenderPrimitiveCollectSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("Primitive");

        // Declare dependencies
        AddDependency<TransformSystem>(); // Needs world transforms
        AddDependency<CameraSystem>();    // Needs camera info
    }

    void RenderPrimitiveCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        // Lazily resolve cameraInfo from CameraSystem if not explicitly set
        // (CameraSystem may be registered after RegisterDefaultEcsSystems runs)
        if (!cameraInfo)
        {
            if (auto cam_sys = world->GetSystem<CameraSystem>())
                cameraInfo = cam_sys->GetCameraInfo();
        }

        if (!cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        cache.cameraInfo = cameraInfo;
        cache.BeginFrame();

        // Get visibility storage for fast O(1) lookup
        VisibilityDataStorage* visibility_storage = nullptr;
        auto vis_system = world->GetSystem<VisibilitySystem>();
        if (vis_system)
        {
            visibility_storage = vis_system->GetStorage();
        }

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        size_t skipped_invisible = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        size_t added = 0;

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp)
                continue;

            const bool tracked_wire = IsTrackedWire(primitiveComp.get());

            if (!primitiveComp->IsVisible() || !primitiveComp->CanRender())
            {
                if (!primitiveComp->IsVisible())
                {
                    ++skipped_invisible;
                }

                if (tracked_wire)
                {
                    auto *owner = primitiveComp->GetOwner();
                    std::fprintf(stderr,
                        "[WireTrace] Collect skip owner='%s': visible=%d can_render=%d has_primitive=%d\n",
                        owner ? owner->GetName().c_str() : "<null>",
                        primitiveComp->IsVisible() ? 1 : 0,
                        primitiveComp->CanRender() ? 1 : 0,
                        primitiveComp->GetPrimitive() ? 1 : 0);
                }
                continue;
            }

            EntityID entity_id = primitiveComp->GetOwnerID();

            // Fast O(1) lookup from VisibilityDataStorage
            if (visibility_storage && visibility_storage->IsInvisible(entity_id))
            {
                ++skipped_invisible;
                if (tracked_wire)
                {
                    auto *owner = primitiveComp->GetOwner();
                    std::fprintf(stderr,
                        "[WireTrace] Collect culled by VisibilitySystem owner='%s'\n",
                        owner ? owner->GetName().c_str() : "<null>");
                }
                continue;
            }

            Entity* entity = primitiveComp->GetOwner();
            if (!entity)
            {
                ++skipped_no_owner;
                if (tracked_wire)
                    std::fprintf(stderr, "[WireTrace] Collect skip: owner entity is null\n");
                continue;
            }

            if (!world->IsEntityRenderEnabled(entity))
            {
                if (tracked_wire)
                {
                    std::fprintf(stderr,
                        "[WireTrace] Collect skip owner='%s': entity render disabled\n",
                        entity->GetName().c_str());
                }
                continue;
            }

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                ++skipped_no_transform;
                if (tracked_wire)
                {
                    std::fprintf(stderr,
                        "[WireTrace] Collect skip owner='%s': missing TransformComponent\n",
                        entity->GetName().c_str());
                }
                continue;
            }

            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            const float distance_to_camera = glm::length(toCamera);
            item->distanceToCamera = distance_to_camera;

            item->UpdateWorldMatrix();

            cache.renderItems.push_back(std::unique_ptr<RenderItem>(std::move(item)));
            cache.renderableCount++;
            ++added;

            if (tracked_wire)
            {
                const auto &slot = primitiveComp->GetMaterialSlot();
                std::fprintf(stderr,
                    "[WireTrace] Collect add owner='%s' rec.id=%s dist=%.3f worldPos=(%.3f,%.3f,%.3f)\n",
                    entity->GetName().c_str(),
                    slot.record ? slot.record->id.c_str() : "<null>",
                    distance_to_camera,
                    worldPos.x,
                    worldPos.y,
                    worldPos.z);
            }
        }

        //if (cache.renderableCount == 0)
        //{
        //    LogInfo("[RenderPrimitiveCollectSystem] No renderables: total=%zu visible=%zu no_owner=%zu no_transform=%zu",
        //             primitives.size(),
        //             added,
        //             skipped_no_owner,
        //             skipped_no_transform);
        //}
    }
}//namespace hgl::ecs

