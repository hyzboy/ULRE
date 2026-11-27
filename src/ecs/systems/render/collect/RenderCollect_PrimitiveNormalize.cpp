#include <hgl/ecs/systems/render/collect/RenderCollectSystem.h>
#include "RenderCollectInternal.h"

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/support/material/ModelTemplateBuilder.h>
#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/ecs/support/material/MaterialVariantTable.h>
#include <hgl/ecs/support/pipeline/RenderStrategyParity.h>
#include <hgl/ecs/support/pipeline/RenderResource.h>
#include <hgl/ecs/support/storage/FrameProfiler.h>
#include <hgl/ecs/core/render/ModelTemplateRenderItem.h>
#include <hgl/ecs/core/render/PrimitiveRenderItem.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/ShadowProxy.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/graph/CameraInfo.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/mesh/GeometryDataBuffer.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    using namespace hgl::ecs::detail::render_collect;

    bool RenderCollectSystem::EnsurePrimitiveModelTemplate(Entity *entity)
    {
        if (!world || !entity)
            return false;

        auto *ref = entity->GetComponent<GPUModelRefComponent>();
        auto *mat_table = world->RenderData().TemplateMaterials();
        auto *gc = world->GetGraphicsContext();
        auto *registry = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;

        // 分支 1：实体本身已经拥有合法的模型模板 ID（例如通过 RegisterModelTemplate 注册的多部件模板）
        if (ref && !ref->IsAutomaticallyNormalized()
         && ref->GetTemplateID() != graph::ssbo::kInvalidModelTemplateID)
        {
            if (const auto *binding = mat_table ? mat_table->FindBinding(ref->GetTemplateID()) : nullptr)
            {
                auto *runtime = GetRuntimeTable(world);
                for (int g = 0; g < binding->groups.GetCount(); ++g)
                {
                    const auto id = binding->groups[g].runtime_row;
                    auto *row = runtime ? runtime->GetMutable(id) : nullptr;
                    if (!row || row->cached_effective_recipe.textures.empty()
                     || materialization.IsTemplateTextureRefreshed(id))
                        continue;
                    const auto old_hash = row->material_texture_configuration_hash;
                    MaterialRuntimeSlot slot;
                    slot.row = id;
                    if (!MaterializeRecipeRowsForPrimitive(entity, slot))
                        return false;
                    materialization.MarkTemplateTextureRefreshed(id);
                    if (old_hash != row->material_texture_configuration_hash)
                        world->MarkTopologyChanged(TopologyChange::MaterialBinding);
                }
                return true;
            }

            // 模板已在 GPU SSBO 中注册，但尚未在 ModelTemplateMaterialTable 中注册渲染绑定（如 GPUModelChessboard 示例）。
            // 绝不能改写其实体模板 ID，而应就地为该已有模板自动补齐 Shading Group 渲染绑定。
            if (!registry || !mat_table)
                return false;

            const auto *tpl_row = registry->GetTypedRow<graph::ssbo::ModelTemplateRow>(
                graph::GlobalSSBOType::ModelTemplatePool, ref->GetTemplateID());
            if (!tpl_row || tpl_row->part_count == 0 || tpl_row->part_offset == 0)
                return false;

            MaterialRuntimeSlot &material_slot = world->GetOrCreateMaterialRuntimeSlot(entity);
            if (!EnsureForwardProgram(entity, material_slot))
                return false;

            MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
            const MaterialRuntimeRow *material_for_item = runtime_table ? runtime_table->Get(material_slot.row) : nullptr;
            if (!material_for_item)
                return false;

            if (material_slot.runtime_dirty || !material_slot.valid
             || material_slot.last_materialize_epoch != materialization.Epoch())
            {
                if (!MaterializeRecipeRowsForPrimitive(entity, material_slot))
                    return false;
                material_slot.valid = true;
            }

            TemplateRenderBinding binding;
            binding.template_id = ref->GetTemplateID();
            if (material_slot.row != INVALID_MATERIAL_RUNTIME_ROW_ID)
                binding.owned_part_rows.Add(material_slot.row);

            const uint32_t shading_group_count = (tpl_row->shading_group_count != 0) ? tpl_row->shading_group_count : 1u;
            const uint32_t b_count = tpl_row->part_bucket_count;
            graph::PrimitiveType primitive_type;
            const graph::GeometryVertexFormat *vertex_format = nullptr;
            GetPrimitiveProgramBuildInputs(entity, primitive_type, vertex_format);

            for (uint32_t g = 0; g < shading_group_count; ++g)
            {
                TemplateShadingGroupDesc grp;
                grp.runtime_row = material_slot.row;
                grp.casts_shadow = CanCastShadow(entity);
                grp.primitive_type = primitive_type;
                grp.vertex_format = vertex_format;

                ValueArray<uint32_t> slot_geo;
                ValueArray<uint32_t> slot_cnt;
                if (b_count > 0)
                {
                    slot_geo.Resize(static_cast<int>(b_count));
                    slot_cnt.Resize(static_cast<int>(b_count));
                    for (uint32_t b = 0; b < b_count; ++b)
                    {
                        slot_geo[b] = 0u;
                        slot_cnt[b] = 0u;
                    }

                    for (uint32_t p = 0; p < tpl_row->part_count; ++p)
                    {
                        const auto *part = registry->GetTypedRow<graph::ssbo::ModelPartRow>(
                            graph::GlobalSSBOType::ModelPartPool, tpl_row->part_offset + p);
                        if (!part || part->geometry_id == 0 || part->bucket_slot >= b_count)
                            continue;
                        if (shading_group_count > 1 && part->shading_group != g)
                            continue;

                        slot_geo[part->bucket_slot] = part->geometry_id;
                        slot_cnt[part->bucket_slot]++;
                    }
                }

                grp.slot_geometry = slot_geo;
                grp.slot_count = slot_cnt;
                binding.groups.Add(grp);
            }

            return mat_table->RegisterBinding(binding);
        }

        return EnsureAutomaticPrimitiveTemplate(entity, ref);
    }

    bool RenderCollectSystem::EnsureAutomaticPrimitiveTemplate(Entity *entity, GPUModelRefComponent *ref)
    {
        auto *gc = world->GetGraphicsContext();
        auto *registry = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;

        // 单图元实体按自身材质 slot 物化，并共享几何模板。
        GeometryData *geom_comp = FindGeometryOf(entity);
        const graph::PrimitiveAsset *prim_asset = geom_comp ? geom_comp->GetPrimitiveAsset() : nullptr;
        graph::Geometry *geo = prim_asset ? prim_asset->GetGeometry() : nullptr;
        if (!geo || !registry)
            return false;

        MaterialRuntimeSlot &material_slot = world->GetOrCreateMaterialRuntimeSlot(entity);
        if (!EnsureForwardProgram(entity, material_slot))
            return false;

        MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        const MaterialRuntimeRow *material_for_item = runtime_table ? runtime_table->Get(material_slot.row) : nullptr;
        if (!material_for_item)
            return false;

        if (material_slot.runtime_dirty || !material_slot.valid
         || material_slot.last_materialize_epoch != materialization.Epoch())
        {
            if (!MaterializeRecipeRowsForPrimitive(entity, material_slot))
                return false;
            material_slot.valid = true;
            material_for_item = runtime_table ? runtime_table->Get(material_slot.row) : nullptr;
            if (!material_for_item)
                return false;
        }

        SinglePrimitiveModelTemplateDesc tmpl_desc;
        tmpl_desc.material_input.runtime_row = material_slot.row;
        tmpl_desc.material_input.material_id = (material_for_item->data_index_row != uint32_t(-1))
            ? material_for_item->data_index_row : 0u;
        tmpl_desc.material_input.texture_id = material_for_item->material_texture_configuration.IsValid()
            ? material_for_item->material_texture_configuration.row_index : 0u;
        tmpl_desc.can_cast_shadow = CanCastShadow(entity);
        tmpl_desc.is_opaque = true;

        const uint32_t auto_template_id = BuildSinglePrimitiveModelTemplate(
            registry, world, prim_asset, tmpl_desc);

        if (auto_template_id == graph::ssbo::kInvalidModelTemplateID)
            return false;

        if (!ref)
            ref = entity->AddComponent<GPUModelRefComponent>();
        if (ref)
        {
            ref->SetTemplateBinding(auto_template_id, true);
        }

        return true;
    }

    // D9：阴影 pass 跳过/失败路径的统一收敛入口（见头文件注释）。
    // reason 包含: "masked caster runtime rows not ready" 等
    bool RenderCollectSystem::AdvanceShadowRetry(
        MaterialRuntimeSlot &slot,
        const char *reason,
        Entity *primitive_comp)
    {
        // A4：计数归**每实例 slot**（该实例在阴影 pass 上的连续跳过帧数）。
        // A3 曾把它记在共享变体记录上——同键的健康兄弟每帧复位，会持续清零失败者的
        // 计数，掩盖 D9 的 masked 失败告警/降频（只影响诊断，不影响渲染）；迁到每实例
        // 侧后各实例独立收敛。
        //
        // 归属判定与 A3 同口径：只有"该实例已经挂上共享行、且阴影变体已登记"才算有
        // 可记账的位置（解析还没走到算键的地方，例如 graphics 缺失时——那类失败本身
        // 已逐帧有独立告警，见 EnsureShadowCasterProgram）。
        const MaterialRuntimeTable *runtime_table = GetRuntimeTable(world);
        const MaterialRuntimeRow *row = runtime_table ? runtime_table->Get(slot.row) : nullptr;
        if (!row || row->shadow_variant == INVALID_MATERIAL_VARIANT_ID)
            return false;

        const uint32_t retries = ++slot.shadow_retry_frames;
        const char *const name = GetOwnerName(primitive_comp);

        if (retries == 1)
        {
            GLogWarning("[RenderCollectSystem] shadow pass skip for '%s': %s -- "
                        "caster excluded from this frame's depth map; static cascade revision is bumped "
                        "every frame for the first %u frames to converge",
                        name, reason, kShadowRetryFullBumpFrames);
        }
        else if (retries == kShadowRetryFullBumpFrames + 1)
        {
            GLogError("[RenderCollectSystem] shadow pass skip for '%s' persisted %u frames (%s) -- "
                      "throttling static cascade redraw to once every %u frames; this caster's shadow stays "
                      "missing meanwhile (check the forward materialization chain)",
                      name, kShadowRetryFullBumpFrames, reason, kShadowRetryBumpPeriod);
        }

        return retries <= kShadowRetryFullBumpFrames
            || (retries % kShadowRetryBumpPeriod) == 0;
    }


}//namespace hgl::ecs
