#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/TransformAccessor.h>

namespace hgl::ecs
{
    // PrimitiveRenderItem implementation
    PrimitiveRenderItem::PrimitiveRenderItem(
        EntityID ent_id,
        const TransformAccessor &trans,
        std::shared_ptr<PrimitiveComponent> prim,
        MaterialRuntimeRowID mat_row,
        ECSContext* ctx)
        : entity_id(ent_id)
        , context(ctx)
        , transform(trans)
        , primitiveComp(prim)
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

    std::shared_ptr<RenderableComponent> PrimitiveRenderItem::GetRenderable() const
    {
        return std::static_pointer_cast<RenderableComponent>(primitiveComp);
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
        // **材质运行期共享行**上（原材质运行期组件已删）。语义与改前一致——
        // 取**前向**变体（ForwardColor purpose）的 program，解析未就绪时退回
        // PrimitiveComponent（非 recipe 图元恒 nullptr）。
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

        return primitiveComp ? primitiveComp->GetShaderProgram() : nullptr;
    }

    hgl::graph::Pipeline* PrimitiveRenderItem::GetPipeline(hgl::graph::RenderPass* render_pass) const
    {
        return primitiveComp ? primitiveComp->GetPipelineForRenderPass(render_pass) : nullptr;
    }

    const hgl::graph::GeometryDataBuffer *PrimitiveRenderItem::GetGeometryDataBuffer() const
    {
        return primitiveComp ? primitiveComp->GetRuntimeGeometryDataBuffer() : nullptr;
    }

    const hgl::graph::GeometryDrawRange *PrimitiveRenderItem::GetGeometryDrawRange() const
    {
        return primitiveComp ? primitiveComp->GetRuntimeGeometryDrawRange() : nullptr;
    }

    TransformPolicySpec PrimitiveRenderItem::GetTransformPolicySpec() const
    {
        return primitiveComp ? primitiveComp->GetTransformPolicySpec() : TransformPolicySpec{};
    }

    PositionSourceSpec PrimitiveRenderItem::GetPositionSourceSpec() const
    {
        return primitiveComp ? primitiveComp->GetPositionSourceSpec() : PositionSourceSpec::MeshVertex;
    }

    graph::RenderItemHandle PrimitiveRenderItem::GetRenderItemHandle() const
    {
        return primitiveComp ? primitiveComp->GetRenderItemHandle() : graph::INVALID_RENDER_ITEM_HANDLE;
    }

    void PrimitiveRenderItem::UpdateWorldMatrix()
    {
        if (transform.IsValid())
        {
            worldMatrix = transform.GetWorldMatrix();
        }
    }
}//namespace hgl::ecs
