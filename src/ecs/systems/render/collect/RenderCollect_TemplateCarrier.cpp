#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include "RenderCollectInternal.h"
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/core/render/ModelTemplateRenderItem.h>
#include <hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
#include <hgl/ecs/support/material/MaterialVariantTable.h>
#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/vk/VKRenderPass.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include <cstring>

namespace hgl::ecs
{
    using namespace hgl::ecs::detail::render_collect;

    bool RenderCollectSystem::CollectTemplateCarrier(Entity *entity,
                                                      const TransformAccessor &transform,
                                                      size_t &added,
                                                      bool &any_material_work,
                                                      uint64_t &shadow_pass_idsum)
    {
        const auto *model_ref = entity->GetComponent<GPUModelRefComponent>();
        if (!model_ref || model_ref->GetTemplateID() == graph::ssbo::kInvalidModelTemplateID)
            return false;

        auto &cache = world->Runner().FrameCache();
        const EntityID entity_id = entity->GetEntityID();
        const char *entity_name = GetOwnerName(entity);
        auto *trace = cache.GetOrCreateTrace(entity_name, entity_id.index, entity_id.generation);
        const auto *template_table = world->RenderData().TemplateMaterials();
        const auto *binding = template_table ? template_table->FindBinding(model_ref->GetTemplateID()) : nullptr;
        if (!binding || binding->groups.IsEmpty())
        {
            any_material_work = true;
            cache.RecordDrop(entity_name, entity_id.index, "RenderCollect", "NoTemplateBinding",
                             "Model template has no render binding");
            return true;
        }

        const bool shadow_pass = world->IsCurrentPassShadow();
        auto *render_target = world->GetRenderTarget();
        auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        MaterialVariantTable *variant_table = world->RenderData().Variants();
        const uint32_t pass_index = shadow_pass ? 1u : 0u;

        const MaterialRuntimeSlot *instance_slot = model_ref->IsAutomaticallyNormalized()
            ? world->RenderData().GetMaterialRuntimeSlot(entity_id) : nullptr;
        MaterialRuntimeRowID active_row_id = INVALID_MATERIAL_RUNTIME_ROW_ID;
        if (model_ref->IsAutomaticallyNormalized())
        {
            if (instance_slot && instance_slot->valid)
                active_row_id = instance_slot->row;
        }
        else
            active_row_id = binding->groups[0].runtime_row;

        const MaterialRuntimeRow *fact_row = runtime_table ? runtime_table->Get(active_row_id) : nullptr;
        uint32_t material_id;
        uint32_t texture_id;
        ResolveMaterialFacts(world, entity_id, fact_row, material_id, texture_id);
        PublishPrimitiveFacts(entity_id, transform.GetID(), 0u, material_id, texture_id, pass_index, nullptr);

        uint32_t collected_groups = 0;
        bool group_failed = false;
        auto drop_group = [&](uint32_t group, const char *reason, const char *message)
        {
            group_failed = true;
            any_material_work = true;
            ++cache.dropped_other;
            cache.RecordDrop(entity_name, entity_id.index, "RenderCollect", reason, message);
            LogWarning(u8"Model template group skipped: owner=%s template=%u group=%u reason=%s",
                       entity_name, model_ref->GetTemplateID(), group, reason);
            if (trace)
                ::strncpy(trace->collect_reason, reason, sizeof(trace->collect_reason) - 1);
        };

        for (uint32_t g = 0; g < static_cast<uint32_t>(binding->groups.GetCount()); ++g)
        {
            const TemplateShadingGroupDesc &grp_desc = binding->groups[g];
            if (shadow_pass && !grp_desc.casts_shadow)
                continue;

            const MaterialRuntimeRowID row_id = model_ref->IsAutomaticallyNormalized()
                ? active_row_id : grp_desc.runtime_row;
            const MaterialRuntimeRow *row = runtime_table ? runtime_table->Get(row_id) : nullptr;
            if (!row)
            {
                drop_group(g, "MissingMaterialRuntimeRow", "Template group material runtime row is unavailable");
                continue;
            }

            graph::PrimitiveType primitive_type;
            const graph::GeometryVertexFormat *vertex_format = grp_desc.vertex_format;
            if (!vertex_format)
                GetPrimitiveProgramBuildInputs(entity, primitive_type, vertex_format);
            graph::ShaderProgram *program = EnsureTemplateGroupProgram(
                world, variant_table, runtime_table, row_id, shadow_pass,
                grp_desc.primitive_type, vertex_format);
            if (!program || !render_pass)
            {
                drop_group(g, "TemplateProgramUnavailable", "Template group program or render pass is unavailable");
                continue;
            }

            graph::mtl::MaterialRecipe effective_recipe = shadow_pass
                ? row->shadow_cached_normalized_recipe : row->cached_effective_recipe;
            if (!shadow_pass && row->recipe_hash == row->cached_effective_recipe_hash)
                effective_recipe = row->cached_normalized_recipe;

            graph::mtl::MaterialPipelineConfig pipeline_config{};
            graph::Pipeline *pipeline = render_pass->CreatePipeline(program, effective_recipe, &pipeline_config);
            if (!pipeline)
            {
                drop_group(g, "TemplatePipelineUnavailable", "Template group pipeline could not be resolved");
                continue;
            }

            world->RenderData().GetMaterialRuntimePipelineTable().Set(entity_id, render_pass, pipeline, pipeline_config, g, program);
            auto item = std::make_unique<ModelTemplateRenderItem>(
                entity_id, transform, row_id, g, model_ref->GetTemplateID(),
                render_pass, pipeline, pipeline_config, world);
            item->UpdateWorldMatrix();
            cache.renderItems.push_back(std::move(item));
            cache.renderableCount++;
            ++added;
            ++cache.collected_items;
            ++collected_groups;
        }

        if (trace)
            ::strncpy(trace->collect_status,
                      group_failed ? (collected_groups ? "partial" : "dropped")
                                   : (collected_groups ? "collected" : "filtered"),
                      sizeof(trace->collect_status) - 1);
        shadow_pass_idsum += (static_cast<uint64_t>(entity_id.index) << 16) | entity_id.generation;
        return true;
    }

}
