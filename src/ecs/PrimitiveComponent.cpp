#include<hgl/ecs/PrimitiveComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/pipeline/VKPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>

namespace hgl::ecs
{
    void PrimitiveComponent::SetPrimitive(hgl::graph::Primitive* prim)
    {
        primitive = prim;
        
        // Update bounding radius based on primitive's bounding volume
        if (primitive)
        {
            const auto& bv = primitive->GetBoundingVolumes();
            
            // Calculate bounding radius from AABB for frustum culling
            // Use the length (diagonal) of the AABB as the bounding radius
            auto extents = bv.aabb.GetLength();
            float radius = extents.Length() * 0.5f; // Half diagonal
            
            SetBoundingRadius(radius);
        }
        else
        {
            SetBoundingRadius(0.0f);
        }
    }

    void PrimitiveComponent::SetOverrideMaterial(hgl::graph::MaterialInstance* mi)
    {
        overrideMaterial = mi;
    }

    hgl::graph::MaterialInstance* PrimitiveComponent::GetMaterialInstance() const
    {
        // Return override material if set, otherwise primitive's material
        if (overrideMaterial)
            return overrideMaterial;

        if (!primitive)
            return nullptr;

        return primitive->GetMaterialInstance();
    }

    hgl::graph::Material* PrimitiveComponent::GetMaterial() const
    {
        // Return override material's base if set
        if (overrideMaterial)
            return overrideMaterial->GetMaterial();

        if (!primitive)
            return nullptr;

        return primitive->GetMaterial();
    }

    hgl::graph::Pipeline* PrimitiveComponent::GetPipeline() const
    {
        if (!primitive)
            return nullptr;

        return primitive->GetPipeline();
    }

    bool PrimitiveComponent::GetLocalAABB(hgl::math::AABB& outAABB) const
    {
        if (!primitive)
            return false;

        const auto& bv = primitive->GetBoundingVolumes();
        outAABB = bv.aabb;
        return true;
    }

    bool PrimitiveComponent::CanRender() const
    {
        return primitive != nullptr && IsVisible();
    }

    void PrimitiveComponent::Render(const glm::mat4& worldMatrix)
    {
        // This is called by RenderCollector or rendering systems
        // The actual rendering would be done through the graphics API
        // Here we just verify we can render
        if (!CanRender())
            return;

        // In a real implementation, this would submit draw commands
        // to a command buffer or render queue using the primitive,
        // material instance, and world matrix
    }

    void PrimitiveComponent::OnAttach()
    {
        RenderableComponent::OnAttach();
        // Additional attachment logic if needed
    }

    void PrimitiveComponent::OnUpdate(float deltaTime)
    {
        RenderableComponent::OnUpdate(deltaTime);
        // Update logic if needed (e.g., animation updates)
    }

    void PrimitiveComponent::OnDetach()
    {
        RenderableComponent::OnDetach();
        
        // Don't delete primitive or material - they're managed externally
        // Just clear our references
        primitive = nullptr;
        overrideMaterial = nullptr;
    }
}//namespace hgl::ecs
