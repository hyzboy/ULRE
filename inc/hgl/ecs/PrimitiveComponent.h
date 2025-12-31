#pragma once

#include<hgl/ecs/RenderableComponent.h>
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

        hgl::graph::Primitive* primitive;              // The primitive to render (not owned)
        hgl::graph::MaterialInstance* overrideMaterial; // Optional material override (not owned)

    public:

        explicit PrimitiveComponent(const std::string& name = "Primitive")
            : RenderableComponent(name)
            , primitive(nullptr)
            , overrideMaterial(nullptr)
        {
        }

        virtual ~PrimitiveComponent() = default;

    public:

        // Primitive management
        void SetPrimitive(hgl::graph::Primitive* prim);
        hgl::graph::Primitive* GetPrimitive() const { return primitive; }

        // Material override
        void SetOverrideMaterial(hgl::graph::MaterialInstance* mi);
        hgl::graph::MaterialInstance* GetOverrideMaterial() const { return overrideMaterial; }
        void ClearOverrideMaterial() { overrideMaterial = nullptr; }

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
