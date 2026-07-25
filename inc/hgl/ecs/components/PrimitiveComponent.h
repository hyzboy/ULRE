#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<glm/glm.hpp>

// Forward declarations to avoid heavy includes
namespace hgl
{
    namespace math
    {
        class AABB;
    }

    namespace graph
    {
        class DescriptorBindingSet;
        class Primitive;
        class MaterialProgram;
        class MaterialInstance;
        class Pipeline;
        class RenderPass;
    }
}

namespace hgl::ecs
{
    /**
     * PrimitiveComponent - Renderable component for static mesh rendering
     *
     * Manages a single Primitive (geometry + material) for rendering.
     * Derived from RenderableComponent to provide rendering capabilities.
     *
     * Features:
     * - Holds reference to hgl::graph::Primitive
     * - Supports MaterialInstance override
     * - Provides access to MaterialProgram, Pipeline, and AABB data
     * - Compatible with RenderCollector for batched rendering
     */
    class PrimitiveComponent : public RenderableComponent
    {
    private:

        hgl::graph::Primitive* primitive;                // The primitive to render (not owned)
        hgl::graph::MaterialInstance* overrideMaterial;   // Optional material override (not owned)
        hgl::graph::DescriptorBindingSet* descriptorBindingSet; // Optional explicit binding set (not owned)
        hgl::graph::Pipeline* overridePipeline;           // Optional pipeline override (not owned)

        // Late-resolve pipeline slot:
        // Populated at render-time if primitive has no pre-baked pipeline.
        hgl::graph::Pipeline* resolvedRuntimePipeline = nullptr; // (not owned)
        hgl::graph::RenderPass* resolvedRuntimeRenderPass = nullptr; // (not owned)
        bool hasPendingPipelinePreset = false;
        hgl::graph::InlinePipeline pendingPipelinePreset = hgl::graph::InlinePipeline::Solid3D;
        void InvalidateResolvedRuntimePipeline();

        PositionSourceSpec positionSourceSpec;            // Unified position source ingress policy
        TransformPolicySpec transformPolicySpec;           // Unified transform policy ingress

    public:

        explicit PrimitiveComponent(const std::string& name = "Primitive")
            : RenderableComponent(name)
            , primitive(nullptr)
            , overrideMaterial(nullptr)
            , descriptorBindingSet(nullptr)
            , overridePipeline(nullptr)
            , positionSourceSpec(PositionSourceSpec::MeshVertex)
            , transformPolicySpec{}
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        const char* GetSystemGroupName() const override { return "Primitive"; }

        void SetPrimitive(hgl::graph::Primitive* prim);
        hgl::graph::Primitive* GetPrimitive() const { return primitive; }

        // MaterialProgram override
        void SetOverrideMaterial(hgl::graph::MaterialInstance* mi);
        hgl::graph::MaterialInstance* GetOverrideMaterial() const { return overrideMaterial; }
        void ClearOverrideMaterial() { SetOverrideMaterial(nullptr); }

        void SetDescriptorBindingSet(hgl::graph::DescriptorBindingSet* set)
        {
            if (descriptorBindingSet != set)
            {
                InvalidateResolvedRuntimePipeline();
            }

            descriptorBindingSet = set;
        }
        hgl::graph::DescriptorBindingSet* GetDescriptorBindingSet() const;
        void ClearDescriptorBindingSet() { SetDescriptorBindingSet(nullptr); }

        void SetOverridePipeline(hgl::graph::Pipeline* p) { overridePipeline = p; }
        hgl::graph::Pipeline* GetOverridePipeline() const { return overridePipeline; }
        void ClearOverridePipeline() { overridePipeline = nullptr; }

        // Late-resolve pipeline request:
        // Call this when creating a Primitive without a pre-baked pipeline.
        // The render path will call RenderPass::CreatePipeline(material, vil, preset) on first batch.
        void RequestPipeline(const hgl::graph::InlinePipeline preset)
        {
            pendingPipelinePreset = preset;
            hasPendingPipelinePreset = true;
        }

        bool HasPendingPipelinePreset() const { return hasPendingPipelinePreset; }
        hgl::graph::InlinePipeline GetPendingPipelinePreset() const { return pendingPipelinePreset; }

        void SetResolvedRuntimePipeline(hgl::graph::Pipeline *p, hgl::graph::RenderPass *rp = nullptr)
        {
            resolvedRuntimePipeline = p;
            resolvedRuntimeRenderPass = rp;
            if(p) hasPendingPipelinePreset = false;
        }

        hgl::graph::Pipeline* GetResolvedRuntimePipeline() const { return resolvedRuntimePipeline; }
        hgl::graph::RenderPass* GetResolvedRuntimeRenderPass() const { return resolvedRuntimeRenderPass; }
        void ClearResolvedRuntimePipeline() { resolvedRuntimePipeline = nullptr; resolvedRuntimeRenderPass = nullptr; hasPendingPipelinePreset = true; }

        void SetTransformPolicySpec(const TransformPolicySpec& spec) { transformPolicySpec = spec; }
        const TransformPolicySpec& GetTransformPolicySpec() const { return transformPolicySpec; }
        void SetPositionSourceSpec(PositionSourceSpec spec) { positionSourceSpec = spec; }
        PositionSourceSpec GetPositionSourceSpec() const { return positionSourceSpec; }

        // MaterialProgram access (returns override if set, otherwise primitive's material)
        hgl::graph::MaterialInstance* GetMaterialInstance() const;
        hgl::graph::MaterialProgram* GetMaterialProgram() const;

        // Pipeline access: override → primitive's pre-baked → runtime resolved
        hgl::graph::Pipeline* GetPipeline() const;

        // Bounding volume
        bool GetLocalAABB(hgl::math::AABB& outAABB) const;

        // Rendering capability check
        bool CanRender() const;

    public:

        // Override RenderableComponent::Render
        void Render(const glm::mat4& worldMatrix) override;

        // Component lifecycle
        void OnAttach() override;
        void OnUpdate(float deltaTime) override;
        void OnDetach() override;
    };
}//namespace hgl::ecs
