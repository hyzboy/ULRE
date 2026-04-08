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
#include<hgl/vk/VKMaterialTemplate.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    namespace
    {
        bool ShouldLogPow2(const uint64_t v)
        {
            return v != 0 && ((v & (v - 1)) == 0);
        }

        uint32_t ComputeGeometryLayoutHash(const graph::Geometry *geo)
        {
            if (!geo)
                return 0;

            const uint32_t count = geo->GetVABCount();
            if (count == 0)
                return 0;

            uint64_t h = 14695981039346656037ULL;
            auto feed = [&](const void *p, size_t n)
            {
                const auto *bytes = reinterpret_cast<const uint8_t*>(p);
                for (size_t i = 0; i < n; ++i)
                    h = (h ^ bytes[i]) * 1099511628211ULL;
            };

            feed(&count, sizeof(count));
            for (uint32_t i = 0; i < count; ++i)
            {
                const graph::VAB *vab = geo->GetVAB(i);
                if (!vab)
                    continue;
                const VkFormat  fmt    = vab->GetFormat();
                const uint32_t  stride = vab->GetStride();
                feed(&fmt,    sizeof(fmt));
                feed(&stride, sizeof(stride));
            }

            return static_cast<uint32_t>((h >> 32) ^ (h & 0xFFFFFFFFu));
        }

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
        size_t skipped_no_primitive = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        size_t deferred_no_resolve = 0;
        size_t added = 0;

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp)
                continue;

            if (!primitiveComp->IsVisible() || !primitiveComp->CanRender())
            {
                if (!primitiveComp->IsVisible())
                    ++skipped_invisible;
                else // visible but CanRender()==false means no primitive set
                    ++skipped_no_primitive;
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
                static uint32_t s_semantic_enter = 0;
                const bool log_semantic_entry = (++s_semantic_enter <= 6u);
                
                auto *gc = world->GetGraphicsContext();
                auto *registry = gc ? gc->GetMaterialAssetRegistry() : nullptr;
                auto *primitive = primitiveComp->GetPrimitive();

                if (log_semantic_entry)
                {
                    LogDebug("[RenderPrimitiveCollect::SemanticResolve] ENTER: prim='%s'  registry=%p  primitive=%p  HasDeferred=%d",
                             primitive ? primitive->GetGeometryName().c_str() : "(no prim)",
                             static_cast<const void*>(registry),
                             static_cast<const void*>(primitive),
                             primitive ? (int)primitive->HasDeferredMI() : -1);
                }

                if (registry && primitive)
                {
                    graph::RuntimeMaterialRequest request;
                    request.domain_id = "";

                    const void *instance_data = nullptr;
                    uint32 instance_data_size = 0;

                    if (primitive->GetMaterialTemplate())
                    {
                        request.pipeline = primitive->GetRenderPreset();

                        // Preserve current MI payload so a cache miss still creates
                        // a correctly initialized runtime MI.
                        if (auto *current_material = primitive->GetMaterialTemplate())
                        {
                            instance_data_size = current_material->GetMIDataBytes();
                            if (instance_data_size > 0)
                                instance_data = primitive->GetMIData();
                        }
                    }
                    else if (primitive->HasDeferredMI())
                    {
                        graph::mtl::MaterialAssetRecord rec;
                        if (registry->QuerySemanticMaterial(primitive->GetDeferredSemanticId(), rec))
                            request.pipeline = rec.pipeline;
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
                    if (auto *material = primitive->GetMaterialTemplate())
                        geometry.primitive = material->GetPrimitiveType();

                    if (primitive->GetVIL())
                    {
                        geometry.vil_hash = ComputeVILHash(primitive->GetVIL());
                    }
                    else if (primitive->HasDeferredMI())
                    {
                        geometry.geometry_for_vil_derivation = primitive->GetGeometry();
                        geometry.vil_hash = primitive->GetDeferredVILHash();
                        geometry.geometry_layout_hash = ComputeGeometryLayoutHash(geometry.geometry_for_vil_derivation);

                        if (!geometry.geometry_for_vil_derivation)
                        {
                            static uint64_t s_deferred_no_geometry = 0;
                            const uint64_t n = ++s_deferred_no_geometry;

                            if (ShouldLogPow2(n))
                            {
                                const uint64_t runtime_entity_id = hgl::ecs::ToRuntimeEntityKey(entity_id);

                                LogWarning("[RenderPrimitiveCollect::ResolveMI] deferred primitive missing geometry_for_vil_derivation: semantic_id=%llu entity_id=%llu prim='%s' deferred_vil_hash=%u total=%llu (fallback to material default VIL)",
                                           static_cast<unsigned long long>(primitiveComp->GetSemanticMaterial()),
                                           static_cast<unsigned long long>(runtime_entity_id),
                                           primitive->GetGeometryName().c_str(),
                                           static_cast<unsigned>(geometry.vil_hash),
                                           static_cast<unsigned long long>(n));
                            }
                        }
                    }

                    {
                        auto slot = registry->ResolveMI(hgl::ecs::ToRuntimeEntityKey(entity_id),
                                                        primitiveComp->GetSemanticMaterial(),
                                                        request,
                                                        geometry,
                                                        instance_data,
                                                        instance_data_size);
                        
                        static uint32_t s_resolve_tick = 0;
                        const bool log_resolve = (++s_resolve_tick <= 6u);
                        if (log_resolve)
                        {
                            LogDebug("[RenderPrimitiveCollect::ResolveMI] semantic_id=%llu  slot.IsValid=%d  slot.material=%p domain=%p mi_id=%d  vil=%p",
                                     static_cast<unsigned long long>(primitiveComp->GetSemanticMaterial()),
                                     (int)slot.IsValid(),
                                     static_cast<const void*>(slot.material_template),
                                     static_cast<const void*>(slot.domain),
                                     (int)slot.mi_id,
                                     static_cast<const void*>(slot.vil));
                        }
                        
                        // Note: ResolveMI may return slot.material_template!=nullptr but mi_id=-1 if slot allocation failed.
                        // Still bind it to set the material on the primitive — mi_id=-1 means no instanced MI data, but we need material/VIL.
                        if (slot.material_template)
                        {
                            primitive->BindMaterialSlot(slot);
                            if (log_resolve)
                                LogDebug("[RenderPrimitiveCollect::BindMaterialSlot] prim='%s' bound. GetMaterialTemplate now=%p mi_id=%d",
                                         primitive->GetGeometryName().c_str(),
                                         static_cast<const void*>(primitive->GetMaterialTemplate()),
                                         (int)slot.mi_id);
                        }
                        else if (log_resolve)
                        {
                            LogWarning("[RenderPrimitiveCollect::ResolveMI] FAILED: no material resolved! semantic_id=%llu",
                                       static_cast<unsigned long long>(primitiveComp->GetSemanticMaterial()));
                        }
                    }
                }
            }

            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            item->distanceToCamera = glm::length(toCamera);

            item->UpdateWorldMatrix();

            auto* primitive = primitiveComp->GetPrimitive();
            if (primitive && primitive->HasDeferredMI() && !semantic_runtime_resolve_enabled)
                ++deferred_no_resolve;

            cache.renderItems.push_back(std::unique_ptr<RenderItem>(std::move(item)));
            cache.renderableCount++;
            ++added;
        }

        // Always emit a summary every frame (throttled: first 5 frames then pow2)
        {
            static uint64_t s_collect_frame = 0;
            const uint64_t n = ++s_collect_frame;
            const bool should_log = (n <= 5) || ((n & (n - 1)) == 0);
            if (should_log || deferred_no_resolve > 0 || (added == 0 && primitives.size() > 0))
            {
                LogInfo("[RenderPrimitiveCollect #%llu] total=%zu  added=%zu  skipped: invisible=%zu no_prim=%zu no_owner=%zu no_transform=%zu  deferred_no_resolve=%zu  semantic_resolve=%s",
                        static_cast<unsigned long long>(n),
                        primitives.size(), added,
                        skipped_invisible, skipped_no_primitive,
                        skipped_no_owner, skipped_no_transform,
                        deferred_no_resolve,
                        semantic_runtime_resolve_enabled ? "ON" : "OFF");

                if (deferred_no_resolve > 0)
                    LogWarning("[RenderPrimitiveCollect] %zu primitive(s) have deferred MI (no material_template) but semantic_runtime_resolve is DISABLED — they will produce no draw calls. "
                               "Call SetSemanticRuntimeResolveEnabled(true) on RenderPrimitiveCollectSystem.",
                               deferred_no_resolve);
            }
        }
    }
}//namespace hgl::ecs

