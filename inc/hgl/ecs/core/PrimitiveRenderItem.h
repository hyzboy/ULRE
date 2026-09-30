#pragma once

#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/support/MaterialRuntimeTable.h>

namespace hgl
{
    namespace graph
    {
        class ShaderProgram;
        class DescriptorBindingSet;
        class Pipeline;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class Entity;
    class ECSContext;
    class PrimitiveComponent;

    /**
     * PrimitiveRenderItem - specialized RenderItem for PrimitiveComponent
     * Similar to hgl::graph::DrawNodePrimitive in the old system
     */
    class PrimitiveRenderItem : public RenderItem
    {
    private:
        EntityID entity_id;
        ECSContext* context = nullptr;
        TransformAccessor transform;          ///< 变换薄句柄（存储行 + 世界；零副本）
        std::shared_ptr<PrimitiveComponent> primitiveComp;

        /// A4：材质运行期**共享行**号（材质绑定状态的唯一载体；原材质运行期组件已删除）。
        /// 行归所属世界的 `MaterialRuntimeTable` 所有，由该图元所属实体的 slot 持引用
        /// （collect → batch 之间不会失效）。
        MaterialRuntimeRowID material_runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID;

        glm::mat4 worldMatrix;

    public:
        PrimitiveRenderItem(
            EntityID ent_id,
            const TransformAccessor &trans,
            std::shared_ptr<PrimitiveComponent> prim,
            MaterialRuntimeRowID mat_row = INVALID_MATERIAL_RUNTIME_ROW_ID,
            ECSContext* ctx = nullptr);

        virtual ~PrimitiveRenderItem() = default;

        // Implement abstract interface
        EntityID GetEntityID() const override { return entity_id; }
        Entity* GetEntity() const override;
        TransformAccessor GetTransform() const override { return transform; }
        std::shared_ptr<RenderableComponent> GetRenderable() const override;
        glm::mat4 GetWorldMatrix() const override { return worldMatrix; }

        // PrimitiveComponent-specific accessors
        std::shared_ptr<PrimitiveComponent> GetPrimitiveComponent() const { return primitiveComp; }

        /// A4：材质运行期共享行号（绑定状态的载体是世界的 `MaterialRuntimeTable`）
        MaterialRuntimeRowID GetMaterialRuntimeRowID() const { return material_runtime_row; }

        /// A4：材质运行期共享行（按行号去世界表取；行无效/已回收 ⇒ nullptr）
        const MaterialRuntimeRow *GetMaterialRuntimeRow() const;

        // ShaderProgram batching interface
        hgl::graph::ShaderProgram* GetShaderProgram() const override;
        hgl::graph::Pipeline* GetPipeline(hgl::graph::RenderPass* render_pass) const override;
        const hgl::graph::GeometryDataBuffer *GetGeometryDataBuffer() const override;
        const hgl::graph::GeometryDrawRange *GetGeometryDrawRange() const override;
        TransformPolicySpec GetTransformPolicySpec() const override;
        PositionSourceSpec GetPositionSourceSpec() const override;

        // RenderItem 4-ID handle
        graph::RenderItemHandle GetRenderItemHandle() const override;

        // Update world matrix from transform
        void UpdateWorldMatrix();
    };
}//namespace hgl::ecs
