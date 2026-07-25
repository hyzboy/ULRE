#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialProgram.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>

namespace hgl::ecs
{
    namespace
    {
        void ResetMaterialRecipe(hgl::graph::mtl::MaterialRecipe &recipe)
        {
            recipe.recipe_name.clear();
            recipe.shading_model = hgl::graph::mtl::ShadingModel::Unknown;
            recipe.preset_hint = hgl::graph::mtl::InvalidMaterialPresetHint;
            recipe.domain.clear();
            recipe.double_sided = false;
            recipe.alpha_test = false;
            recipe.alpha_cutoff = 0.5f;
            recipe.textures.clear();
            recipe.structs.clear();
        }
    }

    void PrimitiveComponent::SetPrimitive(hgl::graph::Primitive* prim)
    {
        if (primitive != prim)
        {
            InvalidateResolvedRuntimePipeline();
        }

        primitive = prim;

        // Update bounding radius based on primitive's bounding volume
        if (primitive)
        {
            const auto& bv = primitive->GetBoundingVolumes();

            // Calculate bounding radius from AABB for frustum culling
            // Use the length (diagonal) of the AABB as the bounding radius
            auto extents = bv.aabb.GetLength();
            float radius = math::Length(extents) * 0.5f; // Half diagonal

            SetBoundingRadius(radius);
        }
        else
        {
            SetBoundingRadius(0.0f);
        }
    }

    void PrimitiveComponent::SetOverrideMaterial(hgl::graph::MaterialInstance* mi)
    {
        if (overrideMaterial != mi)
        {
            InvalidateResolvedRuntimePipeline();
        }

        overrideMaterial = mi;
    }

    void PrimitiveComponent::SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        materialRecipe = recipe;
        hasMaterialRecipe = true;
    }

    const hgl::graph::mtl::MaterialRecipe *PrimitiveComponent::GetMaterialRecipe() const
    {
        return hasMaterialRecipe ? &materialRecipe : nullptr;
    }

    void PrimitiveComponent::ClearMaterialRecipe()
    {
        ResetMaterialRecipe(materialRecipe);
        hasMaterialRecipe = false;
    }

    void PrimitiveComponent::InvalidateResolvedRuntimePipeline()
    {
        const bool runtime_resolve_in_use = (resolvedRuntimePipeline != nullptr) || hasPendingPipelinePreset;

        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;

        if (runtime_resolve_in_use)
        {
            hasPendingPipelinePreset = true;
        }
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

    hgl::graph::DescriptorBindingSet* PrimitiveComponent::GetDescriptorBindingSet() const
    {
        if (descriptorBindingSet)
            return descriptorBindingSet;

        if (!primitive)
            return nullptr;

        return primitive->GetDescriptorBindingSet();
    }

    hgl::graph::MaterialProgram* PrimitiveComponent::GetMaterialProgram() const
    {
        if (overrideMaterial)
            return overrideMaterial->GetMaterialProgram();

        if (primitive)
        {
            auto *prim_mat = primitive->GetMaterialProgram();
            if (prim_mat)
                return prim_mat;
        }

        // Fallback only when primitive path has no material.
        if (descriptorBindingSet)
            return descriptorBindingSet->GetMaterialProgram();

        return nullptr;
    }

    hgl::graph::Pipeline* PrimitiveComponent::GetPipeline() const
    {
        if (overridePipeline)
            return overridePipeline;

        if (primitive && primitive->GetPipeline())
            return primitive->GetPipeline();

        // Return runtime-resolved pipeline if available (late-resolve path).
        return resolvedRuntimePipeline;
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
        descriptorBindingSet = nullptr;
        overridePipeline = nullptr;
        ResetMaterialRecipe(materialRecipe);
        hasMaterialRecipe = false;
        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;
        hasPendingPipelinePreset = false;
    }
}//namespace hgl::ecs
