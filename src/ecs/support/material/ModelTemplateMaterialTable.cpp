#include <hgl/ecs/support/material/ModelTemplateMaterialTable.h>
#include <hgl/ecs/core/Context.h>
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

        if (bindings.ContainsKey(binding.template_id))
        {
            GLogWarning(u8"[ModelTemplateMaterialTable] RegisterBinding: template_id=%u already registered, replacing", binding.template_id);
            UnregisterBinding(binding.template_id);
        }

        // 登记时对全部持有的部件材质行加引用计数
        if (context)
        {
            MaterialRuntimeTable *runtime_table = context->GetMaterialRuntimeTable();
            if (runtime_table)
            {
                for (int i = 0; i < binding.owned_part_rows.GetCount(); ++i)
                {
                    const MaterialRuntimeRowID row_id = binding.owned_part_rows[i];
                    if (row_id != INVALID_MATERIAL_RUNTIME_ROW_ID)
                    {
                        runtime_table->AddRef(row_id);
                    }
                }
            }
        }

        auto *copy = new TemplateRenderBinding();
        copy->template_id = binding.template_id;
        copy->owned_part_rows = binding.owned_part_rows;
        for (int i = 0; i < binding.groups.GetCount(); ++i)
        {
            copy->groups.Add(binding.groups[i]);
        }

        bindings.Add(binding.template_id, copy);
        return true;
    }

    bool ModelTemplateMaterialTable::UnregisterBinding(uint32_t template_id)
    {
        TemplateRenderBinding **existing_ptr = bindings.GetValuePointer(template_id);
        if (!existing_ptr || !*existing_ptr)
            return false;

        TemplateRenderBinding *existing = *existing_ptr;
        ReleaseBindingRows(*existing);
        delete existing;
        bindings.DeleteByKey(template_id);
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

    void ModelTemplateMaterialTable::Clear()
    {
        for (auto &pair : bindings)
        {
            if (pair.second)
            {
                ReleaseBindingRows(*pair.second);
                delete pair.second;
            }
        }
        bindings.Clear();
    }
}
