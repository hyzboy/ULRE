#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    void GPUModelRefComponent::OnDetach()
    {
        ClearOverrideMaterialRow();
    }

    bool GPUModelRefComponent::SetOverrideMaterialRow(const MaterialRuntimeRowID row)
    {
        if (row == override_material_row)
            return true;
        auto *context = owner ? owner->GetContextAs<hgl::ecs::ECSContext>() : nullptr;
        auto *runtime = context ? context->GetMaterialRuntimeTable() : nullptr;
        if (!runtime || (row != INVALID_MATERIAL_RUNTIME_ROW_ID && !runtime->Get(row)))
        {
            GLogError(u8"[GPUModelRefComponent] Override rejected: unavailable runtime row=%u", row);
            return false;
        }
        if (row != INVALID_MATERIAL_RUNTIME_ROW_ID)
            runtime->AddRef(row);
        const auto previous = override_material_row;
        override_material_row = row;
        if (previous != INVALID_MATERIAL_RUNTIME_ROW_ID)
            runtime->Release(previous);
        context->InvalidateEntityRuntimePipeline(owner->GetEntityID());
        return true;
    }

    void GPUModelRefComponent::ClearOverrideMaterialRow()
    {
        SetOverrideMaterialRow(INVALID_MATERIAL_RUNTIME_ROW_ID);
    }
}
