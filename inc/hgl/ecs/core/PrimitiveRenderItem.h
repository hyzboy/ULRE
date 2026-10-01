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
        class RenderPass;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class Entity;
    class ECSContext;

    /**
     * PrimitiveRenderItem - specialized RenderItem for a primitive entity
     *
     * A5b：图元渲染组件（普通 / 多实例）已删除 ⇒ 本项不再持有
     * 组件指针，改持**实体身份 + 世界**（entity_id / context）：材质绑定经世界
     * `MaterialRuntimeTable` 的共享行号取用，几何/资产与每实例状态按实体现查
     * （GeometryData 组件 / 每实例 slot）。
     */
    class PrimitiveRenderItem : public RenderItem
    {
    protected:
        EntityID entity_id;
        ECSContext* context = nullptr;
        TransformAccessor transform;          ///< 变换薄句柄（存储行 + 世界；零副本）

        /// A4：材质运行期**共享行**号（材质绑定状态的唯一载体；原材质运行期组件已删除）。
        /// 行归所属世界的 `MaterialRuntimeTable` 所有，由该图元所属实体的 slot 持引用
        /// （collect → batch 之间不会失效）。
        MaterialRuntimeRowID material_runtime_row = INVALID_MATERIAL_RUNTIME_ROW_ID;

        glm::mat4 worldMatrix;

        /// 本实体在世界材质运行期表里的每实例 slot（无则 nullptr）——每实例状态唯一载体
        const MaterialRuntimeSlot *GetRuntimeSlot() const;

    public:
        PrimitiveRenderItem(
            EntityID ent_id,
            const TransformAccessor &trans,
            MaterialRuntimeRowID mat_row = INVALID_MATERIAL_RUNTIME_ROW_ID,
            ECSContext* ctx = nullptr);

        virtual ~PrimitiveRenderItem() = default;

        // Implement abstract interface
        EntityID GetEntityID() const override { return entity_id; }
        Entity* GetEntity() const override;
        TransformAccessor GetTransform() const override { return transform; }
        glm::mat4 GetWorldMatrix() const override { return worldMatrix; }

        /// A4：材质运行期共享行号（绑定状态的载体是世界的 `MaterialRuntimeTable`）
        MaterialRuntimeRowID GetMaterialRuntimeRowID() const { return material_runtime_row; }

        /// A4：材质运行期共享行（按行号去世界表取；行无效/已回收 ⇒ nullptr）
        const MaterialRuntimeRow *GetMaterialRuntimeRow() const;

        // ShaderProgram batching interface
        hgl::graph::ShaderProgram* GetShaderProgram() const override;
        hgl::graph::Pipeline* GetPipeline(hgl::graph::RenderPass* render_pass) const override;
        const hgl::graph::GeometryDataBuffer *GetGeometryDataBuffer() const override;
        const hgl::graph::GeometryDrawRange *GetGeometryDrawRange() const override;

        // RenderItem 4-ID handle
        graph::RenderItemHandle GetRenderItemHandle() const override;

        // Update world matrix from transform
        void UpdateWorldMatrix();
    };
}//namespace hgl::ecs
