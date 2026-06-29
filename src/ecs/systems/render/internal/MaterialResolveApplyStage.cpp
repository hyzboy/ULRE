#include "MaterialResolveApplyStage.h"

#include <hgl/log/Log.h>

namespace hgl::ecs::internal
{
    void ApplyResolvedMIBuckets(
        std::vector<ResolveTask> &tasks,
        const std::vector<ResolvedMIBucket> &resolved_buckets,
        graph::PrimitiveManager *prim_mgr,
        ApplyStageStats &out_stats,
        uint32_t &inout_resolve_fail_count)
    {
        out_stats = ApplyStageStats{};

        if (!prim_mgr)
            return;

        for (const auto &resolved_bucket : resolved_buckets)
        {
            graph::MaterialBindingInstance *mi = resolved_bucket.mi;
            const graph::VIL *resolve_vil = resolved_bucket.resolve_vil;
            graph::ProgramInstanceBinding *resolved_program_binding = resolved_bucket.resolved_program_binding.get();
            graph::MaterialInstancePayload *resolved_payload = resolved_bucket.resolved_payload.get();

            for (const size_t idx : resolved_bucket.task_indices)
            {
                ResolveTask &task = tasks[idx];
                graph::Primitive *previous_primitive = task.comp->GetPrimitive();
                graph::Geometry *previous_unresolved_geometry = task.comp->GetUnresolvedGeometry();

                task.slot->resolved_binding_instance = mi;
                task.slot->resolved_material = graph::MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
                task.slot->resolved_program_binding = resolved_program_binding;
                task.slot->resolved_program = task.slot->resolved_material;
                task.slot->resolved_payload = resolved_payload;
                task.slot->resolved_binding_id = resolved_program_binding ? resolved_program_binding->id : 0;
                task.slot->resolved_payload_id = resolved_payload ? resolved_payload->id : 0;
                task.slot->resolved_domain = graph::MaterialBindingInstanceInternalAccess::GetDomain(mi);
                task.slot->resolved_domain_id = graph::MaterialBindingInstanceInternalAccess::GetDomainID(mi);
                task.slot->resolved_vil = resolve_vil;
                task.slot->resolved_mi_id = mi->GetMIID();
                task.slot->resolved_preset = mi->GetRenderPreset();
                task.slot->dirty = false;

                GLogInfo("[ECS::MaterialResolveSystem] resolved_output comp=%p mi=%p material=%p material_prim=%u resolved_preset=%u domain_id=%u vil=%p",
                        task.comp.get(),
                        mi,
                        task.slot->resolved_material,
                        task.slot->resolved_material ? static_cast<unsigned>(task.slot->resolved_material->GetPrimitiveType()) : 0u,
                        static_cast<unsigned>(task.slot->resolved_preset),
                        static_cast<unsigned>(task.slot->resolved_domain_id),
                        resolve_vil);
                GLogInfo("[ECS::MaterialResolveSystem] resolved_output note: staging_render_state is not populated here; committed path requires a later staging/commit writer for comp=%p",
                        task.comp.get());
                ++out_stats.resolved_count;

                // Stage-2: unresolved geometry still creates Primitive in-place.
                if (task.comp->GetUnresolvedGeometry())
                {
                    if (auto *prim = prim_mgr->CreatePrimitive(task.geometry, mi, resolve_vil))
                    {
                        task.comp->SetPrimitive(prim);
                        task.comp->SetUnresolvedGeometry(nullptr);
                        ++out_stats.primitive_created_count;
                    }
                }
                else if (auto *existing_prim = task.comp->GetPrimitive())
                {
                    if (existing_prim->ChangeMaterialInstance(mi))
                    {
                        ++out_stats.primitive_updated_count;
                    }
                    else
                    {
                        auto *replacement = prim_mgr->CreatePrimitive(task.geometry, mi, resolve_vil);
                        if (replacement)
                        {
                            task.comp->SetPrimitive(replacement);
                            prim_mgr->Release(existing_prim);
                            ++out_stats.primitive_recreated_count;
                        }
                        else
                        {
                            ++inout_resolve_fail_count;
                        }
                    }
                }

                auto *resolved_primitive = task.comp->GetPrimitive();
                if (resolved_primitive)
                {
                    PrimitiveComponent::ResolvedMaterialState staged_state{};
                    staged_state.program_binding = task.slot->resolved_program_binding;
                    staged_state.program = task.slot->resolved_program ? task.slot->resolved_program : task.slot->resolved_material;
                    staged_state.payload = task.slot->resolved_payload;
                    staged_state.binding_id = task.slot->resolved_binding_id;
                    staged_state.payload_id = task.slot->resolved_payload_id;
                    staged_state.binding_instance = task.slot->resolved_binding_instance;
                    staged_state.material = staged_state.program ? staged_state.program : task.slot->resolved_material;
                    staged_state.domain = task.slot->resolved_domain;
                    staged_state.domain_id = task.slot->resolved_domain_id;
                    staged_state.vil = task.slot->resolved_vil;
                    staged_state.mi_id = task.slot->resolved_mi_id;
                    staged_state.preset = task.slot->resolved_preset;

                    task.comp->SetStagingRenderState(staged_state, resolved_primitive);

                    GLogInfo("[ECS::MaterialResolveSystem] staged_render_state comp=%p primitive=%p mi=%p material=%p vil=%p domain=%p preset=%u",
                            task.comp.get(),
                            resolved_primitive,
                            staged_state.binding_instance,
                            staged_state.material,
                            staged_state.vil,
                            staged_state.domain,
                            static_cast<unsigned>(staged_state.preset));
                }
                else
                {
                    GLogInfo("[ECS::MaterialResolveSystem] skip staging: resolved primitive is null for comp=%p",
                            task.comp.get());
                }

                if (previous_unresolved_geometry
                 && previous_unresolved_geometry != task.comp->GetUnresolvedGeometry()
                 && (!task.comp->GetPrimitive() || previous_unresolved_geometry != task.comp->GetPrimitive()->GetGeometry()))
                    delete previous_unresolved_geometry;
            }
        }
    }
}
