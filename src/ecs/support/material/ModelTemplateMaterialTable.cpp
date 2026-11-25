#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/asset/ModelTemplateManager.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    ModelTemplateMaterialTable::~ModelTemplateMaterialTable()
    {
        Clear();
    }

    void ModelTemplateMaterialTable::ReleaseBindingRows(const TemplateRenderBinding &binding)
    {
        if (!context)
            return;

        MaterialRuntimeTable *runtime_table = context->GetMaterialRuntimeTable();
        if (!runtime_table)
            return;

        for (int i = 0; i < binding.owned_part_rows.GetCount(); ++i)
        {
            const MaterialRuntimeRowID row_id = binding.owned_part_rows[i];
            if (row_id != INVALID_MATERIAL_RUNTIME_ROW_ID)
            {
                runtime_table->Release(row_id);
            }
        }
    }

    bool ModelTemplateMaterialTable::RegisterBinding(const TemplateRenderBinding &binding)
    {
        if (binding.template_id == 0)
        {
            GLogError(u8"[ModelTemplateMaterialTable] RegisterBinding failed: template_id is 0");
            return false;
        }

        MaterialRuntimeTable *runtime_table = context ? context->GetMaterialRuntimeTable() : nullptr;
        if (!runtime_table || binding.groups.IsEmpty())
        {
            GLogError(u8"[ModelTemplateMaterialTable] RegisterBinding rejected: missing runtime table or groups");
            return false;
        }

        for (int i = 0; i < binding.owned_part_rows.GetCount(); ++i)
        {
            const auto row = binding.owned_part_rows[i];
            if (!runtime_table->Get(row))
            {
                GLogError(u8"[ModelTemplateMaterialTable] RegisterBinding rejected: dead owned row=%u", row);
                return false;
            }
            for (int j = 0; j < i; ++j)
            {
                if (binding.owned_part_rows[j] != row)
                    continue;
                GLogError(u8"[ModelTemplateMaterialTable] RegisterBinding rejected: duplicate owned row=%u", row);
                return false;
            }
        }
        for (int g = 0; g < binding.groups.GetCount(); ++g)
        {
            const auto &group = binding.groups[g];
            if (!binding.owned_part_rows.Contains(group.runtime_row)
             || group.slot_geometry.GetCount() != group.slot_count.GetCount())
            {
                GLogError(u8"[ModelTemplateMaterialTable] RegisterBinding rejected: invalid group=%d", g);
                return false;
            }
        }

        // The source may itself belong to this table; copy before releasing it.
        auto *copy = binding_storage.Create();
        copy->template_id = binding.template_id;
        const auto *previous = FindBinding(binding.template_id);
        copy->owns_gpu_rows = binding.owns_gpu_rows || (previous && previous->owns_gpu_rows);
        copy->owned_part_rows = binding.owned_part_rows;
        for (int i = 0; i < binding.groups.GetCount(); ++i)
        {
            copy->groups.Add(binding.groups[i]);
        }

        for (int i = 0; i < copy->owned_part_rows.GetCount(); ++i)
            runtime_table->AddRef(copy->owned_part_rows[i]);

        const uint32_t template_id = copy->template_id;
        if (bindings.ContainsKey(template_id))
            UnregisterBinding(template_id);
        bindings.Add(template_id, copy);
        context->MarkTopologyChanged(TopologyChange::ModelTemplate);
        return true;
    }

    bool ModelTemplateMaterialTable::UnregisterBinding(uint32_t template_id)
    {
        TemplateRenderBinding **existing_ptr = bindings.GetValuePointer(template_id);
        if (!existing_ptr || !*existing_ptr)
            return false;

        TemplateRenderBinding *existing = *existing_ptr;
        if (IsGPUQuiescent())
        {
            ReleaseBindingRows(*existing);
            binding_storage.Remove(existing);
        }
        else
            retired_bindings.Add(existing);
        bindings.DeleteByKey(template_id);
        if (context)
            context->MarkTopologyChanged(TopologyChange::ModelTemplate);

        if (const SinglePrimitiveTemplateKey *k = template_to_single_key.GetValuePointer(template_id))
        {
            single_primitive_cache.DeleteByKey(*k);
            template_to_single_key.DeleteByKey(template_id);
        }

        return true;
    }

    const TemplateRenderBinding *ModelTemplateMaterialTable::FindBinding(uint32_t template_id) const
    {
        TemplateRenderBinding *const *ptr = bindings.GetValuePointer(template_id);
        return ptr ? *ptr : nullptr;
    }

    bool ModelTemplateMaterialTable::HasBinding(uint32_t template_id) const
    {
        return bindings.ContainsKey(template_id);
    }

    bool ModelTemplateMaterialTable::IsGPUQuiescent() const
    {
        if (!context || !context->GetGraphicsContext())
            return true;
        auto *target = context->GetRenderTarget();
        auto *queue = target ? target->GetQueue() : nullptr;
        return queue && queue->IsLastSubmitComplete();
    }

    void ModelTemplateMaterialTable::CollectRetiredTemplates()
    {
        if (!IsGPUQuiescent())
            return;
        for (auto *binding : retired_bindings)
        {
            ReleaseBindingRows(*binding);
            binding_storage.Remove(binding);
        }
        retired_bindings.Clear();
        auto *graphics = context ? context->GetGraphicsContext() : nullptr;
        auto *registry = graphics ? graphics->GetGlobalSSBOBufferRegistry() : nullptr;
        if (!registry)
            return;
        // The graphics submission waits for culling; a completed fence also covers
        // those template reads. World-local frame counters are not completion epochs.
        if (auto *pool = registry->GetPool(graph::GlobalSSBOType::ModelTemplatePool))
            pool->CollectRecyclable(UINT64_MAX, nullptr, context);
        if (auto *pool = registry->GetPool(graph::GlobalSSBOType::ModelPartPool))
            pool->CollectRecyclable(UINT64_MAX, nullptr, context);
    }

    uint32_t ModelTemplateMaterialTable::FindSinglePrimitiveTemplate(const SinglePrimitiveTemplateKey &key) const
    {
        const uint32_t *id_ptr = single_primitive_cache.GetValuePointer(key);
        return id_ptr ? *id_ptr : 0u;
    }

    void ModelTemplateMaterialTable::RegisterSinglePrimitiveTemplate(const SinglePrimitiveTemplateKey &key, uint32_t template_id)
    {
        if (template_id == 0)
            return;

        single_primitive_cache.Add(key, template_id);
        template_to_single_key.Add(template_id, key);
    }

    void ModelTemplateMaterialTable::Clear()
    {
        if (bindings.IsEmpty() && retired_bindings.IsEmpty())
            return;
        if (!IsGPUQuiescent())
        {
            GLogError(u8"[ModelTemplateMaterialTable] Clear requires completed GPU submissions");
            return;
        }
        auto *graphics = context ? context->GetGraphicsContext() : nullptr;
        auto *registry = graphics ? graphics->GetGlobalSSBOBufferRegistry() : nullptr;
        for (auto &pair : bindings)
        {
            if (pair.second)
            {
                if (registry && pair.second->owns_gpu_rows)
                    graph::ModelTemplateManager::RetireModelTemplate(
                        registry, pair.first, context->GetRenderSubmissionSerial()
                                            + graph::MaterialTextureConfigurationRetireEpochDelay, context);
                ReleaseBindingRows(*pair.second);
            }
        }
        bindings.Clear();
        for (auto *binding : retired_bindings)
            ReleaseBindingRows(*binding);
        retired_bindings.Clear();
        binding_storage.Clear();
        single_primitive_cache.Clear();
        template_to_single_key.Clear();
        CollectRetiredTemplates();
    }
}
