#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/graph/render/RenderItemDescriptor.h>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class ShaderProgram;
        class DescriptorBindingSet;
        class Pipeline;
        class RenderPass;
        struct GeometryDataBuffer;
        struct GeometryDrawRange;
    }

    namespace ecs
    {
        class Entity;
    }
}

namespace hgl::ecs
{
    // Forward declarations
    class RenderableComponent;

    /**
     * Base RenderItem class - abstract interface for rendering
     * Similar to hgl::graph::DrawNode in the old system
     */
    class RenderItem
    {
    public:
        uint32_t index = 0;                      // Index in batch
        uint32_t transform_index = 0;            // Transform index in buffer

        float distanceToCamera = 0.0f;           // Distance to camera for sorting
        bool isVisible = true;                   // Visibility flag

        virtual ~RenderItem() = default;

        // Abstract interface - returns EntityID and entity pointer
        virtual EntityID GetEntityID() const = 0;
        virtual Entity* GetEntity() const = 0;
        virtual std::shared_ptr<TransformComponent> GetTransform() const = 0;
        virtual std::shared_ptr<RenderableComponent> GetRenderable() const = 0;
        virtual glm::mat4 GetWorldMatrix() const = 0;

        // For material batching support
        virtual hgl::graph::ShaderProgram* GetShaderProgram() const = 0;
        // 管线按 RenderPass 维度解析：同一世界渲染到多个 RT（如 ShadowMap 的
        // depth-only 离屏 Pass）时，各 RT 使用各自格式匹配的管线。
        virtual hgl::graph::Pipeline* GetPipeline(hgl::graph::RenderPass* render_pass) const = 0;
        virtual const hgl::graph::GeometryDataBuffer *GetGeometryDataBuffer() const = 0;
        virtual const hgl::graph::GeometryDrawRange *GetGeometryDrawRange() const = 0;

        // Unified transform ingress for R08 (behavior stays unchanged until strategy rollout)
        virtual TransformPolicySpec GetTransformPolicySpec() const { return TransformPolicySpec{}; }
        virtual PositionSourceSpec GetPositionSourceSpec() const { return PositionSourceSpec::MeshVertex; }

        // RenderItem 4-ID handle
        virtual graph::RenderItemHandle GetRenderItemHandle() const { return graph::INVALID_RENDER_ITEM_HANDLE; }

        // Comparison for sorting
        virtual int Compare(const RenderItem& other) const;
    };

}//namespace hgl::ecs
