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
        bool NeedsPrimitiveSlotBind(const graph::Primitive *prim, const graph::PrimitiveMaterialSlot &slot)
        {
            if (!prim)
                return false;

            // Deferred primitives must bind once to materialize runtime draw state.
            if (prim->HasDeferredMI())
                return true;

            if (prim->GetMaterialTemplate() != slot.material_template)
                return true;
            if (prim->GetDomainHandle() != slot.domain_handle)    // P9: handle comparison
                return true;
            if (prim->GetMIID() != slot.mi_id)
                return true;
            if (prim->GetVIL() != slot.vil)
                return true;
            if (prim->GetRenderPreset() != slot.preset)
                return true;
            if (prim->GetMaterialPreset() != slot.material_preset)
                return true;

            return false;
        }

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

        uint64_t ComputePrimitiveVariantHash(graph::SemanticMaterialId semantic_id,
                                             const graph::RuntimeMaterialRequest &request,
                                             const graph::GeometrySignature &geometry)
        {
            uint64_t h = 14695981039346656037ULL;
            auto feed = [&](const void *p, size_t n)
            {
                const auto *bytes = reinterpret_cast<const uint8_t*>(p);
                for (size_t i = 0; i < n; ++i)
                    h = (h ^ bytes[i]) * 1099511628211ULL;
            };

            auto feed_str = [&](const std::string &s)
            {
                feed(s.data(), s.size());
                const uint8_t z = 0;
                feed(&z, 1);
            };

            feed(&semantic_id, sizeof(semantic_id));

            {
                const uint8_t pipeline = static_cast<uint8_t>(request.pipeline);
                feed(&pipeline, 1);
                feed_str(request.domain_id);
                feed(&request.policy_flags, sizeof(request.policy_flags));
                feed(&request.transparency_mode, sizeof(request.transparency_mode));
                feed(&request.lod_tier, sizeof(request.lod_tier));
            }

            {
                const uint8_t prim = static_cast<uint8_t>(geometry.primitive);
                feed(&prim, 1);
                feed(&geometry.vil_hash, sizeof(geometry.vil_hash));
                // geometry_layout_hash is a pre-VIL discriminator for deferred primitives
                // (vil_hash == 0).  Once a VIL has been derived the material-required attribs
                // are fully captured in vil_hash; including geometry_layout_hash would create
                // spurious cache misses for primitives that share the same material VIL but
                // differ only in extra geometry attributes the material does not use.
                if (geometry.vil_hash == 0)
                    feed(&geometry.geometry_layout_hash, sizeof(geometry.geometry_layout_hash));
            }

            return h;
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
        size_t slot_bind_attempt = 0;
        size_t slot_bind_success = 0;
        size_t slot_bind_noop = 0;
        size_t slot_bind_skip_domain_direct = 0;
        size_t slot_bind_failed = 0;

        material_cache.BeginFrame();

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

            graph::Primitive *primitive_for_semantic = primitiveComp->GetPrimitive();
            const graph::SemanticMaterialId semantic_id = primitiveComp->HasSemanticMaterial()
                                                       ? primitiveComp->GetSemanticMaterial()
                                                       : ((primitive_for_semantic && primitive_for_semantic->HasDeferredMI())
                                                            ? primitive_for_semantic->GetDeferredSemanticId()
                                                            : graph::SemanticMaterialId(0));

            if (semantic_runtime_resolve_enabled && semantic_id != 0)
            {
                static uint32_t s_semantic_enter = 0;
                const bool log_semantic_entry = (++s_semantic_enter <= 6u);
                graph::PrimitiveMaterialSlot resolved_slot_snapshot;
                bool has_resolved_slot_snapshot = false;
                
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
                        geometry.geometry_for_vil_derivation = primitive->GetGeometry();
                        if (geometry.geometry_for_vil_derivation)
                            geometry.geometry_layout_hash = ComputeGeometryLayoutHash(geometry.geometry_for_vil_derivation);
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
                                               static_cast<unsigned long long>(semantic_id),
                                           static_cast<unsigned long long>(runtime_entity_id),
                                           primitive->GetGeometryName().c_str(),
                                           static_cast<unsigned>(geometry.vil_hash),
                                           static_cast<unsigned long long>(n));
                            }
                        }
                    }

                    {
                        const uint64_t variant_hash = ComputePrimitiveVariantHash(
                            semantic_id, request, geometry);

                        // Level-1 probe: has this variant been resolved by any entity this session?
                        const bool l1_hit = material_cache.ProbeVariant(variant_hash);

                        // ── Handle-aware fast path ──
                        // If the Primitive carries a pre-allocated MaterialInstanceHandle,
                        // complete its binding (material + VIL) and build the slot directly,
                        // bypassing ResolveMI's MI slot allocation.
                        const uint64_t deferred_handle = primitive->GetDeferredMIHandle();
                        graph::PrimitiveMaterialSlot slot;

                        if (deferred_handle != 0)
                        {
                            graph::mtl::MaterialAssetRecord rec;
                            bool handle_ok = false;
                            if (registry->QuerySemanticMaterial(semantic_id, rec))
                            {
                                auto domain_handle = registry->Acquire(rec);
                                if (domain_handle.IsValid() && domain_handle.material)
                                {
                                    const graph::VIL *resolved_vil = registry->ResolveVIL(
                                        domain_handle.material, rec, primitive->GetGeometry());
                                    if (resolved_vil)
                                    {
                                        registry->CompleteBinding(deferred_handle,
                                                                  domain_handle.material,
                                                                  resolved_vil,
                                                                  request.pipeline);
                                        handle_ok = registry->BuildSlot(deferred_handle, slot);
                                    }
                                }
                            }

                            if (!handle_ok)
                            {
                                // Fallback: handle path failed, use normal ResolveMI.
                                slot = registry->ResolveMI(hgl::ecs::ToRuntimeEntityKey(entity_id),
                                                           semantic_id,
                                                           request,
                                                           geometry,
                                                           instance_data,
                                                           instance_data_size);
                            }
                        }
                        else
                        {
                        slot = registry->ResolveMI(hgl::ecs::ToRuntimeEntityKey(entity_id),
                                                        semantic_id,
                                                        request,
                                                        geometry,
                                                        instance_data,
                                                        instance_data_size);
                        }
                        
                        static uint32_t s_resolve_tick = 0;
                        const bool log_resolve = (++s_resolve_tick <= 6u);
                        if (log_resolve)
                        {
                            // Phase 3 diagnostic fields: semantic_id, entity_id, vil_hash,
                            // geometry_layout_hash, fallback_count (slot.vil==null counts as one).
                            const uint32_t fallback_count = (slot.material_template && !slot.vil) ? 1u : 0u;
                            LogDebug("[RenderPrimitiveCollect::ResolveMI] "
                                     "semantic_id=%llu entity_id=%llu "
                                     "vil_hash=%u geometry_layout_hash=%u "
                                     "slot.IsValid=%d slot.material=%p domain=%p mi_id=%d vil=%p fallback=%u",
                                     static_cast<unsigned long long>(semantic_id),
                                     static_cast<unsigned long long>(hgl::ecs::ToRuntimeEntityKey(entity_id)),
                                     static_cast<unsigned>(geometry.vil_hash),
                                     static_cast<unsigned>(geometry.geometry_layout_hash),
                                     (int)slot.IsValid(),
                                     static_cast<const void*>(slot.material_template),
                                     static_cast<const void*>(slot.domain),
                                     (int)slot.mi_id,
                                     static_cast<const void*>(slot.vil),
                                     fallback_count);
                        }
                        
                        // Note: ResolveMI may return slot.material_template!=nullptr but mi_id=-1 if slot allocation failed.
                        // Still bind it to set the material on the primitive — mi_id=-1 means no instanced MI data, but we need material/VIL.
                        if (slot.material_template)
                        {
                            const bool use_domain_direct_slot = domain_direct_mi_ssbo_enabled
                                                              && slot.domain != nullptr
                                                              && slot.mi_id >= 0;

                            bool did_bind_slot = false;

                            // Level-2 probe: did this Primitive already bind this variant
                            // with matching geometry?  Only meaningful when L1 confirmed
                            // the variant is globally valid.
                            const bool l2_hit = !use_domain_direct_slot
                                              && l1_hit
                                              && material_cache.ProbePrimitiveBinding(
                                                     primitive, variant_hash,
                                                     geometry.geometry_layout_hash);

                            if (use_domain_direct_slot)
                            {
                                ++slot_bind_skip_domain_direct;
                                material_cache.MarkVariantResolved(variant_hash);
                                material_cache.MarkPrimitiveBound(primitive, variant_hash,
                                                                  geometry.geometry_layout_hash);
                            }
                            else if (l2_hit)
                            {
                                // Primitive already has the correct variant bound; skip BindMaterialSlot.
                                ++slot_bind_noop;
                            }
                            else if (NeedsPrimitiveSlotBind(primitive, slot))
                            {
                                ++slot_bind_attempt;
                                did_bind_slot = primitive->BindMaterialSlot(slot,"collect");
                                if (!did_bind_slot)
                                {
                                    ++slot_bind_failed;
                                    material_cache.ErasePrimitiveBinding(primitive);
                                    LogWarning("[RenderPrimitiveCollect::BindMaterialSlot] FAILED: prim='%s' material=%p domain=%p mi_id=%d",
                                               primitive->GetGeometryName().c_str(),
                                               static_cast<const void*>(slot.material_template),
                                               static_cast<const void*>(slot.domain),
                                               (int)slot.mi_id);
                                }
                                else
                                {
                                    ++slot_bind_success;
                                    material_cache.MarkVariantResolved(variant_hash);
                                    material_cache.MarkPrimitiveBound(primitive, variant_hash,
                                                                      geometry.geometry_layout_hash);
                                }
                            }
                            else
                            {
                                ++slot_bind_noop;
                                material_cache.MarkVariantResolved(variant_hash);
                                material_cache.MarkPrimitiveBound(primitive, variant_hash,
                                                                  geometry.geometry_layout_hash);
                            }

                            resolved_slot_snapshot = slot;
                            has_resolved_slot_snapshot = true;

                            if (log_resolve)
                                LogDebug("[RenderPrimitiveCollect::BindMaterialSlot] prim='%s' %s. GetMaterialTemplate now=%p mi_id=%d",
                                         primitive->GetGeometryName().c_str(),
                                         use_domain_direct_slot ? "skipped(shared-primitive-safe)"
                                         : (did_bind_slot ? "bound" : "noop(already-matched)"),
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

                auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

                if (domain_direct_mi_ssbo_enabled && has_resolved_slot_snapshot)
                {
                    const int override_mi_id = primitiveComp ? primitiveComp->GetMIIDOverride() : -1;
                    if (override_mi_id >= 0 && resolved_slot_snapshot.domain)
                        resolved_slot_snapshot.mi_id = override_mi_id;

                    item->SetEntityMaterialBinding(resolved_slot_snapshot);
                }

                glm::vec3 worldPos = transform->GetWorldPosition();
                item->worldPosition = worldPos;
                glm::vec3 toCamera = worldPos - camera_pos;
                item->distanceToCamera = glm::length(toCamera);

                item->UpdateWorldMatrix();

                if (primitive && primitive->HasDeferredMI() && !semantic_runtime_resolve_enabled)
                    ++deferred_no_resolve;

                cache.renderItems.push_back(std::unique_ptr<RenderItem>(std::move(item)));
                cache.renderableCount++;
                ++added;
                continue;
            }

            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

            if (domain_direct_mi_ssbo_enabled)
            {
                auto *primitive_for_slot = primitiveComp->GetPrimitive();
                const int override_mi_id = primitiveComp ? primitiveComp->GetMIIDOverride() : -1;
                const int resolved_mi_id = override_mi_id >= 0
                                         ? override_mi_id
                                         : (primitive_for_slot ? primitive_for_slot->GetMIID() : -1);

                if (primitive_for_slot && primitive_for_slot->GetDomain() && resolved_mi_id >= 0)
                {
                    graph::PrimitiveMaterialSlot resolved_slot_snapshot;
                    resolved_slot_snapshot.material_template = primitive_for_slot->GetMaterialTemplate();
                    resolved_slot_snapshot.domain = primitive_for_slot->GetDomain();
                    resolved_slot_snapshot.domain_handle = primitive_for_slot->GetDomainHandle();   // P9
                    resolved_slot_snapshot.mi_id = resolved_mi_id;
                    resolved_slot_snapshot.vil = primitive_for_slot->GetVIL();
                    resolved_slot_snapshot.preset = primitive_for_slot->GetRenderPreset();
                    resolved_slot_snapshot.texture_array_slot_flags = resolved_slot_snapshot.material_template
                        ? resolved_slot_snapshot.material_template->GetTextureArraySlotFlags()
                        : 0;
                    resolved_slot_snapshot.mit_data = reinterpret_cast<const uint32_t*>(primitive_for_slot->GetMITData());
                    resolved_slot_snapshot.mit_data_count = primitive_for_slot->GetMITDataBytes() / sizeof(uint32_t);
                    resolved_slot_snapshot.material_preset = primitive_for_slot->GetMaterialPreset();
                    item->SetEntityMaterialBinding(resolved_slot_snapshot);
                }
            }

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
            bool should_log = false;
            switch (bind_slot_summary_log_mode)
            {
                case BindSlotSummaryLogMode::Off:
                    should_log = false;
                    break;
                case BindSlotSummaryLogMode::EveryFrame:
                    should_log = true;
                    break;
                default:
                    should_log = (n <= 5) || ((n & (n - 1)) == 0);
                    break;
            }
            if (should_log || deferred_no_resolve > 0 || (added == 0 && primitives.size() > 0))
            {
                LogInfo("[RenderPrimitiveCollect #%llu] total=%zu  added=%zu  skipped: invisible=%zu no_prim=%zu no_owner=%zu no_transform=%zu  deferred_no_resolve=%zu  semantic_resolve=%s",
                        static_cast<unsigned long long>(n),
                        primitives.size(), added,
                        skipped_invisible, skipped_no_primitive,
                        skipped_no_owner, skipped_no_transform,
                        deferred_no_resolve,
                        semantic_runtime_resolve_enabled ? "ON" : "OFF");

                LogInfo("[RenderPrimitiveCollect::BindSlotSummary] attempt=%zu success=%zu noop=%zu skip_domain_direct=%zu failed=%zu",
                    slot_bind_attempt,
                    slot_bind_success,
                    slot_bind_noop,
                    slot_bind_skip_domain_direct,
                    slot_bind_failed);

                LogInfo("[RenderPrimitiveCollect::MaterialCache] l1_hit=%zu l1_miss=%zu l2_bind_skip=%zu geometry_invalidate=%zu",
                    material_cache.GetFrameL1HitCount(),
                    material_cache.GetFrameL1MissCount(),
                    material_cache.GetFrameL2BindSkipCount(),
                    material_cache.GetFrameGeometryInvalidateCount());

                if (deferred_no_resolve > 0)
                    LogWarning("[RenderPrimitiveCollect] %zu primitive(s) have deferred MI (no material_template) but semantic_runtime_resolve is DISABLED — they will produce no draw calls. "
                               "Call SetSemanticRuntimeResolveEnabled(true) on RenderPrimitiveCollectSystem.",
                               deferred_no_resolve);
            }
        }
    }
}//namespace hgl::ecs

