#include<hgl/ecs/RenderPrimitiveBatchSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/BoundingBoxComponent.h>
#include<hgl/ecs/MaterialBatch.h>
#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/ecs/PrimitiveRenderItem.h>
#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/TransformSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/VKDevice.h>

namespace hgl::ecs
{
    RenderPrimitiveBatchSystem::RenderPrimitiveBatchSystem(const std::string& name)
        : System(name)
    {
    }

    void RenderPrimitiveBatchSystem::Update(float /*deltaTime*/)
    {
        if (!world || !cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        if (cache.renderItems.empty())
            return;

        if (frustumCullingEnabled)
            PerformFrustumCulling();

        if (distanceSortingEnabled)
            SortByDistance();

        TransformSystem* transform_system = nullptr;
        if (auto system = world->GetSystem<TransformSystem>())
        {
            transform_system = system.get();
            transform_system->SetDevice(device);
            transform_system->EnsureTransformBuffer();
            transform_system->RefreshHandleOrder();
        }

        AssignTransformIndices(transform_system);

        if (batchingEnabled)
        {
            BuildMaterialBatches();
            FinalizeBatches();
        }
    }

    void RenderPrimitiveBatchSystem::PerformFrustumCulling()
    {
        auto& cache = world->GetRenderFrameCache();

        frustum.SetMatrix(cameraInfo->vp);

        for (auto& itemPtr : cache.renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto entity = item->GetEntity();
            if (!entity)
                continue;

            auto bbox = entity->GetComponent<BoundingBoxComponent>();
            if (bbox)
            {
                const glm::vec3 local_center = bbox->GetCenter();
                const glm::vec3 local_extents = bbox->GetExtents();
                const float radius = glm::length(local_extents);

                const glm::mat4 worldMat = item->GetWorldMatrix();
                const glm::vec3 world_center = glm::vec3(worldMat * glm::vec4(local_center, 1.0f));

                item->isVisible = (frustum.SphereIn(world_center, radius) != math::Frustum::Scope::OUTSIDE);
            }
            else
            {
                auto primitiveComp = item->GetPrimitiveComponent();
                if (!primitiveComp)
                    continue;

                auto transform = item->GetTransform();
                if (!transform)
                    continue;

                glm::vec3 worldPos = transform->GetWorldPosition();
                float boundingRadius = primitiveComp->GetBoundingRadius();

                if (boundingRadius <= 0.0f)
                    item->isVisible = true;
                else
                    item->isVisible = (frustum.SphereIn(worldPos, boundingRadius) != math::Frustum::Scope::OUTSIDE);
            }
        }
    }

    void RenderPrimitiveBatchSystem::SortByDistance()
    {
        auto& cache = world->GetRenderFrameCache();

        std::sort(cache.renderItems.begin(), cache.renderItems.end(),
            [](const std::unique_ptr<PrimitiveRenderItem>& a, const std::unique_ptr<PrimitiveRenderItem>& b) {
                return a->distanceToCamera < b->distanceToCamera;
            });
    }

    void RenderPrimitiveBatchSystem::AssignTransformIndices(TransformSystem* transform_system)
    {
        auto& cache = world->GetRenderFrameCache();
        uint32_t static_count = 0;
        uint32_t dynamic_count = 0;
        uint32_t dynamic_base = 0;

        if (transform_system)
        {
            static_count = transform_system->GetStaticCount();
            dynamic_count = transform_system->GetDynamicCount();
            dynamic_base = transform_system->GetDynamicBaseIndex(static_count, dynamic_count);
        }

        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item)
                continue;

            auto transform = item->GetTransform();
            if (!transform)
                continue;

            const auto handle = transform->GetStorageHandle();
            if (handle == TransformDataStorage::INVALID_HANDLE)
            {
                item->transform_index = 0;
                continue;
            }

            uint32_t group_index = 0;
            if (transform_system &&
                transform_system->TryGetTransformGroupIndex(handle, transform->IsMovable(), group_index))
            {
                item->transform_index = transform->IsMovable() ? (dynamic_base + group_index) : group_index;
            }
            else
            {
                item->transform_index = 0;
            }
        }
    }

    void RenderPrimitiveBatchSystem::BuildMaterialBatches()
    {
        auto& cache = world->GetRenderFrameCache();

        ECSTransformAssignmentBuffer* shared_transform_buffer = nullptr;
        if (auto transform_system = world->GetSystem<TransformSystem>())
        {
            shared_transform_buffer = transform_system->GetTransformBuffer();
        }

        for (auto& itemPtr : cache.renderItems)
        {
            PrimitiveRenderItem* item = itemPtr.get();
            if (!item || !item->isVisible)
                continue;

            auto* material = item->GetMaterial();
            auto* pipeline = item->GetPipeline();

            if (!material || !pipeline)
                continue;

            MaterialPipelineKey key(material, pipeline);
            auto it = cache.materialBatches.find(key);

            if (it == cache.materialBatches.end())
            {
                auto batch = std::make_unique<MaterialBatch>(key, device);
                batch->SetCameraInfo(cameraInfo);
                batch->SetTransformBuffer(shared_transform_buffer);
                batch->AddItem(item);
                cache.materialBatches[key] = std::move(batch);
            }
            else
            {
                it->second->SetTransformBuffer(shared_transform_buffer);
                it->second->AddItem(item);
            }
        }
    }

    void RenderPrimitiveBatchSystem::FinalizeBatches()
    {
        auto& cache = world->GetRenderFrameCache();

        for (auto& pair : cache.materialBatches)
        {
            if (pair.second)
                pair.second->Finalize();
        }
    }
}//namespace hgl::ecs
