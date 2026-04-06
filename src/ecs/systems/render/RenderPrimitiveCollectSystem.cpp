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
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    namespace
    {
        uint32_t ComputeVILHash(const graph::VIL *vil)
        {
            if (!vil)
                return 0;

            uint64_t h = 14695981039346656037ULL;
            auto feed = [&](const void *p, size_t n)
            {
                const auto *bytes = reinterpret_cast<const uint8_t*>(p);
                for (size_t i = 0; i < n; ++i)
                    h = (h ^ bytes[i]) * 1099511628211ULL;
            };

            const uint32_t count = vil->GetVertexAttribCount();
            feed(&count, sizeof(count));

            const auto *vif = vil->GetVIFList();
            if (vif && count > 0)
                feed(vif, sizeof(graph::VertexInputFormat) * count);

            return static_cast<uint32_t>((h >> 32) ^ (h & 0xFFFFFFFFu));
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

            if (!primitiveComp->IsVisible() || !primitiveComp->CanRender())
            {
                if (!primitiveComp->IsVisible())
                {
                    ++skipped_invisible;
                }
                continue;
            }

            EntityID entity_id = primitiveComp->GetOwnerID();

            // Fast O(1) lookup from VisibilityDataStorage
            if (visibility_storage && visibility_storage->IsInvisible(entity_id))
            {
                ++skipped_invisible;
                continue;
            }

            Entity* entity = primitiveComp->GetOwner();
            if (!entity)
            {
                ++skipped_no_owner;
                continue;
            }

            if (!world->IsEntityRenderEnabled(entity))
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                ++skipped_no_transform;
                continue;
            }

            if (semantic_runtime_resolve_enabled && primitiveComp->HasSemanticMaterial())
            {
                auto *gc = world->GetGraphicsContext();
                auto *registry = gc ? gc->GetMaterialAssetRegistry() : nullptr;
                auto *primitive = primitiveComp->GetPrimitive();

                if (registry && primitive)
                {
                    graph::RuntimeMaterialRequest request;
                    request.domain_id = "";

                    const void *instance_data = nullptr;
                    uint32 instance_data_size = 0;

                    auto *current_mi = primitiveComp->GetMaterialInstance();
                    if (current_mi)
                    {
                        request.pipeline = current_mi->GetRenderPreset();

                        // Preserve current MI payload so a cache miss still creates
                        // a correctly initialized runtime MI.
                        if (auto *current_material = current_mi->GetMaterial())
                        {
                            instance_data_size = current_material->GetMIDataBytes();
                            if (instance_data_size > 0)
                                instance_data = current_mi->GetMIData();
                        }
                    }

                    // Runtime auto-transparency decision source (distance-based, 3D only).
                    const glm::vec3 world_pos = transform->GetWorldPosition();
                    const float distance = glm::length(world_pos - camera_pos);
                    if (auto_transparency_enabled
                     && request.pipeline == graph::GraphicsPipelinePreset::Solid3D
                     && distance < auto_transparency_near_distance)
                    {
                        if (use_real_alpha3d_enabled)
                        {
                            request.transparency_mode = 3; // blend
                            request.pipeline = graph::GraphicsPipelinePreset::Alpha3D;
                        }
                        else
                        {
                            request.transparency_mode = 2; // dither
                            request.pipeline = graph::GraphicsPipelinePreset::Dither3D;
                        }
                    }
                    else
                    {
                        request.transparency_mode = 0; // opaque
                    }

                    graph::GeometrySignature geometry;
                    if (auto *material = primitive->GetMaterial())
                        geometry.primitive = material->GetPrimitiveType();

                    if (current_mi)
                        geometry.vil_hash = ComputeVILHash(current_mi->GetVIL());

                    if (auto *resolved = registry->ResolveMI(hgl::ecs::ToRuntimeEntityKey(entity_id),
                                                             primitiveComp->GetSemanticMaterial(),
                                                             request,
                                                             geometry,
                                                             instance_data,
                                                             instance_data_size))
                        primitiveComp->SetOverrideMaterial(resolved);
                }
            }

            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            item->distanceToCamera = glm::length(toCamera);

            item->UpdateWorldMatrix();

            cache.renderItems.push_back(std::unique_ptr<RenderItem>(std::move(item)));
            cache.renderableCount++;
            ++added;
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

