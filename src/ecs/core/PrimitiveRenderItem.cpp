#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/PrimitiveState.h>
#include<hgl/ecs/support/MaterialVariantTable.h>
#include<hgl/ecs/support/TransformAccessor.h>

namespace hgl::ecs
{
    // PrimitiveRenderItem implementation
    PrimitiveRenderItem::PrimitiveRenderItem(
        EntityID ent_id,
        const TransformAccessor &trans,
        MaterialRuntimeRowID mat_row,
        ECSContext* ctx)
        : entity_id(ent_id)
        , context(ctx)
        , transform(trans)
        , material_runtime_row(mat_row)
        , worldMatrix(1.0f)
    {
        if (transform.IsValid())
        {
            worldMatrix = transform.GetWorldMatrix();
        }
    }

    Entity* PrimitiveRenderItem::GetEntity() const
    {
        if (!context || !entity_id.IsValid())
            return nullptr;
        return context->GetEntity(entity_id);
    }

    const MaterialRuntimeSlot *PrimitiveRenderItem::GetRuntimeSlot() const
    {
        const MaterialRuntimeTable *runtime_table = context ? context->GetMaterialRuntimeTable() : nullptr;

        return runtime_table ? runtime_table->GetSlot(entity_id) : nullptr;
    }

    const MaterialRuntimeRow *PrimitiveRenderItem::GetMaterialRuntimeRow() const
    {
        if (!context || material_runtime_row == INVALID_MATERIAL_RUNTIME_ROW_ID)
            return nullptr;

        const MaterialRuntimeTable *runtime_table = context->GetMaterialRuntimeTable();

        return runtime_table ? runtime_table->Get(material_runtime_row) : nullptr;
    }

    hgl::graph::ShaderProgram* PrimitiveRenderItem::GetShaderProgram() const
    {
        // A3：program 在世界的材质变体表里（按变体 ID 取）；变体 ID 自 A4 起挂在
        // **材质运行期共享行**上。语义与改前一致——取**前向**变体（ForwardColor purpose）
        // 的 program。A5b：原图元组件 GetShaderProgram 恒返回 nullptr 的回退已删。
        const MaterialRuntimeRow *row = GetMaterialRuntimeRow();

        if (row && context)
        {
            const MaterialVariantTable *variant_table = context->GetMaterialVariantTable();
            const MaterialVariantRecord *record = variant_table
                ? variant_table->Get(row->forward_variant)
                : nullptr;

            if (record && record->program)
                return record->program;
        }

        return nullptr;
    }

    hgl::graph::Pipeline* PrimitiveRenderItem::GetPipeline(hgl::graph::RenderPass* render_pass) const
    {
        // A5b：运行期管线归每实例（collect 阶段按当前 pass 解析）；
        // B5-b 起落世界级旁表（键 = 本实体），读时比对 pass —— 语义与旧 slot 字段逐字一致。
        return context
            ? context->GetMaterialRuntimePipelineTable().Get(entity_id, render_pass)
            : nullptr;
    }

    graph::RenderItemHandle PrimitiveRenderItem::GetRenderItemHandle() const
    {
        // A5b：4-ID 句柄归每实例 slot（经世界访问器取；未分配 ⇒ INVALID）
        return context
            ? hgl::ecs::GetRenderItemHandle(*context, entity_id)
            : graph::INVALID_RENDER_ITEM_HANDLE;
    }

    void PrimitiveRenderItem::UpdateWorldMatrix()
    {
        if (transform.IsValid())
        {
            worldMatrix = transform.GetWorldMatrix();
        }
    }
}//namespace hgl::ecs
