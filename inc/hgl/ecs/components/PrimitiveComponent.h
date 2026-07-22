#pragma once

#include<hgl/ecs/components/RenderableComponent.h>
#include<hgl/ecs/support/PositionSourceSpec.h>
#include<hgl/ecs/support/TransformPolicySpec.h>
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
        class Material;
        class MaterialInstance;
        class Pipeline;
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
     * - Provides access to Material, Pipeline, and AABB data
     * - Compatible with RenderCollector for batched rendering
     */
    class PrimitiveComponent : public RenderableComponent
    {
    private:

        hgl::graph::Primitive* primitive;                // The primitive to render (not owned)
        hgl::graph::MaterialInstance* overrideMaterial;   // Optional material override (not owned)
        hgl::graph::DescriptorBindingSet* descriptorBindingSet; // Optional explicit binding set (not owned)
        hgl::graph::Pipeline* overridePipeline;           // Optional pipeline override (not owned)
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

        // Material override
        void SetOverrideMaterial(hgl::graph::MaterialInstance* mi);
        hgl::graph::MaterialInstance* GetOverrideMaterial() const { return overrideMaterial; }
        void ClearOverrideMaterial() { overrideMaterial = nullptr; }

        void SetDescriptorBindingSet(hgl::graph::DescriptorBindingSet* set) { descriptorBindingSet = set; }
        hgl::graph::DescriptorBindingSet* GetDescriptorBindingSet() const;
        void ClearDescriptorBindingSet() { descriptorBindingSet = nullptr; }

        void SetOverridePipeline(hgl::graph::Pipeline* p) { overridePipeline = p; }
        hgl::graph::Pipeline* GetOverridePipeline() const { return overridePipeline; }
        void ClearOverridePipeline() { overridePipeline = nullptr; }

        void SetTransformPolicySpec(const TransformPolicySpec& spec) { transformPolicySpec = spec; }
        const TransformPolicySpec& GetTransformPolicySpec() const { return transformPolicySpec; }
        void SetPositionSourceSpec(PositionSourceSpec spec) { positionSourceSpec = spec; }
        PositionSourceSpec GetPositionSourceSpec() const { return positionSourceSpec; }

        // Material access (returns override if set, otherwise primitive's material)
        hgl::graph::MaterialInstance* GetMaterialInstance() const;
        hgl::graph::Material* GetMaterial() const;

        // Pipeline access
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
