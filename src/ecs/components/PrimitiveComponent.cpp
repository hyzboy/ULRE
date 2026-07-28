#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialProgram.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
        std::string BuildTextureResourceId(hgl::graph::Texture *texture)
        {
            if (!texture)
                return {};

            return "texid:" + std::to_string(texture->GetID());
        }

        void ResetMaterialRecipe(hgl::graph::mtl::MaterialRecipe &recipe)
        {
            recipe.recipe_name.clear();
            recipe.shading_model = hgl::graph::mtl::ShadingModel::Unknown;
            recipe.preset_hint = hgl::graph::mtl::InvalidMaterialPresetHint;
            recipe.base_material_info_name.clear();
            recipe.domain.clear();
            recipe.coordinate_system_2d = hgl::graph::CoordinateSystem2D::NDC;
            recipe.local_to_world_2d = true;
            recipe.material_lod = 0;
            recipe.material_quality_tier = 0;
            recipe.double_sided = false;
            recipe.alpha_test = false;
            recipe.alpha_cutoff = 0.5f;
            recipe.textures.clear();
            recipe.structs.clear();
            recipe.ssbo_assets.clear();
        }

        void ResetMaterialTextureAuthoringResource(PrimitiveComponent::MaterialTextureAuthoringResource &resource)
        {
            resource.resource_id.clear();
            resource.texture = nullptr;
            resource.sampler = nullptr;
            resource.kind = PrimitiveComponent::MaterialTextureResourceKind::Texture2D;
            resource.direct_value = 0;
            resource.use_direct_value = false;
            resource.required = false;
        }

        void ResetMaterialStructAuthoringResource(PrimitiveComponent::MaterialStructAuthoringResource &resource)
        {
            resource.ssbo_type = hgl::graph::mtl::SSBOType::UserDefined;
            resource.ssbo_id = 0;
            resource.buffer = nullptr;
            resource.element_capacity = 0;
            resource.byte_stride = 0;
            resource.struct_index = 0;
            resource.use_struct_index = false;
            resource.shared_across_instances = false;
            resource.authored = false;
        }

        void ResetMaterialStructNamedAuthoringResource(PrimitiveComponent::MaterialStructNamedAuthoringResource &resource)
        {
            resource.ssbo_name.clear();
            resource.ssbo_id = 0;
            resource.struct_index = 0;
            resource.use_struct_index = false;
            resource.shared_across_instances = false;
            resource.authored = false;
        }
    }

    void PrimitiveComponent::SetPrimitive(hgl::graph::Primitive* prim)
    {
        if (primitive != prim)
        {
            InvalidateResolvedRuntimePipeline();
        }

        primitive = prim;
        runtimePrimitiveFromAsset = false;

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

    void PrimitiveComponent::SetInternalAssetRuntimePrimitive(hgl::graph::Primitive *prim)
    {
        if (primitive != prim)
            InvalidateResolvedRuntimePipeline();

        primitive = prim;
        runtimePrimitiveFromAsset = (prim != nullptr);

        if (primitive)
        {
            const auto &bv = primitive->GetBoundingVolumes();
            auto extents = bv.aabb.GetLength();
            float radius = math::Length(extents) * 0.5f;
            SetBoundingRadius(radius);
        }
        else if (primitiveAsset && primitiveAsset->GetGeometry())
        {
            const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
            auto extents = bv.aabb.GetLength();
            float radius = math::Length(extents) * 0.5f;
            SetBoundingRadius(radius);
        }
        else
        {
            SetBoundingRadius(0.0f);
        }
    }

    void PrimitiveComponent::SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset)
    {
        if (primitiveAsset != asset)
            InvalidateResolvedRuntimePipeline();

        primitiveAsset = asset;

        if (primitiveAsset && primitiveAsset->GetMaterialRecipe())
        {
            overrideMaterial = nullptr;
            descriptorBindingSet = nullptr;
        }

        if (!primitive && primitiveAsset && primitiveAsset->GetGeometry())
        {
            const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
            auto extents = bv.aabb.GetLength();
            float radius = math::Length(extents) * 0.5f;
            SetBoundingRadius(radius);
        }
        else if (!primitive)
        {
            SetBoundingRadius(0.0f);
        }
    }

    void PrimitiveComponent::SetInternalOverrideMaterial(hgl::graph::MaterialInstance* mi)
    {
        if (HasMaterialRecipe() && mi)
        {
            GLogWarning("[PrimitiveComponent] Ignore SetInternalOverrideMaterial while recipe runtime is enabled.");
            return;
        }

        if (overrideMaterial != mi)
        {
            InvalidateResolvedRuntimePipeline();
        }

        overrideMaterial = mi;
    }

    void PrimitiveComponent::SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        InvalidateResolvedRuntimePipeline();
        // Recipe runtime is exclusive with legacy per-node overrides.
        overrideMaterial = nullptr;
        descriptorBindingSet = nullptr;
        materialRecipe = recipe;
        hasMaterialRecipe = true;
    }

    const hgl::graph::mtl::MaterialRecipe *PrimitiveComponent::GetMaterialRecipe() const
    {
        if (hasMaterialRecipe)
            return &materialRecipe;

        if (!primitiveAsset)
            return nullptr;

        if (const auto *variant = primitiveAsset->GetVariant(primitiveVariantIndex))
            return variant->material_recipe;

        return primitiveAsset->GetMaterialRecipe();
    }

    void PrimitiveComponent::ClearMaterialRecipe()
    {
        InvalidateResolvedRuntimePipeline();
        ResetMaterialRecipe(materialRecipe);
        hasMaterialRecipe = false;
        ClearMaterialAuthoringResources();
    }

    void PrimitiveComponent::SetMaterialTextureResource(hgl::graph::mtl::TextureSlot slot,
                                                        hgl::graph::Texture *texture,
                                                        hgl::graph::Sampler *sampler,
                                                        MaterialTextureResourceKind kind,
                                                        const std::string &resource_id,
                                                        bool required)
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialTextureResources.size())
            return;

        auto &resource = materialTextureResources[index];
        if (!texture || !sampler)
        {
            ResetMaterialTextureAuthoringResource(resource);
            return;
        }

        resource.texture = texture;
        resource.sampler = sampler;
        resource.kind = kind;
        resource.direct_value = 0;
        resource.use_direct_value = false;
        resource.required = required;
        resource.resource_id = resource_id.empty() ? BuildTextureResourceId(texture) : resource_id;
    }

    void PrimitiveComponent::SetMaterialTextureValue(hgl::graph::mtl::TextureSlot slot, uint32_t value)
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialTextureResources.size())
            return;

        auto &resource = materialTextureResources[index];
        ResetMaterialTextureAuthoringResource(resource);
        resource.direct_value = value;
        resource.use_direct_value = true;
    }

    const PrimitiveComponent::MaterialTextureAuthoringResource *PrimitiveComponent::GetMaterialTextureResource(hgl::graph::mtl::TextureSlot slot) const
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialTextureResources.size())
            return nullptr;

        const auto &resource = materialTextureResources[index];
        return (resource.use_direct_value || (resource.texture && resource.sampler)) ? &resource : nullptr;
    }

    void PrimitiveComponent::ClearMaterialTextureResource(hgl::graph::mtl::TextureSlot slot)
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialTextureResources.size())
            return;

        ResetMaterialTextureAuthoringResource(materialTextureResources[index]);
    }

    void PrimitiveComponent::SetMaterialStructResource(hgl::graph::mtl::DataSlot slot,
                                                       hgl::graph::mtl::SSBOType ssbo_type,
                                                       uint32_t ssbo_id,
                                                       hgl::graph::DeviceBuffer *buffer,
                                                       uint32_t element_capacity,
                                                       uint32_t byte_stride,
                                                       uint32_t struct_index,
                                                       bool use_struct_index,
                                                       bool shared_across_instances)
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialStructResources.size())
            return;

        auto &resource = materialStructResources[index];
        const bool empty_authoring =
            !buffer &&
            ssbo_type == hgl::graph::mtl::SSBOType::UserDefined &&
            ssbo_id == 0 &&
            element_capacity == 0 &&
            byte_stride == 0 &&
            struct_index == 0 &&
            !use_struct_index &&
            !shared_across_instances;

        if (empty_authoring)
        {
            ResetMaterialStructAuthoringResource(resource);
            return;
        }

        resource.ssbo_type = ssbo_type;
        resource.ssbo_id = ssbo_id;
        resource.buffer = buffer;
        resource.element_capacity = element_capacity;
        resource.byte_stride = byte_stride;
        resource.struct_index = struct_index;
        resource.use_struct_index = use_struct_index;
        resource.shared_across_instances = shared_across_instances;
        resource.authored = true;
    }

    const PrimitiveComponent::MaterialStructAuthoringResource *PrimitiveComponent::GetMaterialStructResource(hgl::graph::mtl::DataSlot slot) const
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialStructResources.size())
            return nullptr;

        const auto &resource = materialStructResources[index];
        return resource.authored ? &resource : nullptr;
    }

    void PrimitiveComponent::SetMaterialStructResource(const MaterialStructNamedAuthoringResource &resource)
    {
        if (resource.ssbo_name.empty() || resource.ssbo_id == 0)
        {
            ClearMaterialStructResource(resource.ssbo_name);
            return;
        }

        for (auto &existing : materialStructNamedResources)
        {
            if (existing.ssbo_name != resource.ssbo_name)
                continue;

            existing.ssbo_id = resource.ssbo_id;
            existing.struct_index = resource.struct_index;
            existing.use_struct_index = resource.use_struct_index;
            existing.shared_across_instances = resource.shared_across_instances;
            existing.authored = true;
            return;
        }

        MaterialStructNamedAuthoringResource named = resource;
        named.authored = true;
        materialStructNamedResources.emplace_back(std::move(named));
    }

    const PrimitiveComponent::MaterialStructNamedAuthoringResource *PrimitiveComponent::GetMaterialStructResource(const std::string &ssbo_name) const
    {
        if (ssbo_name.empty())
            return nullptr;

        for (const auto &resource : materialStructNamedResources)
        {
            if (resource.authored && resource.ssbo_name == ssbo_name)
                return &resource;
        }

        return nullptr;
    }

    void PrimitiveComponent::ClearMaterialStructResource(const std::string &ssbo_name)
    {
        if (ssbo_name.empty())
            return;

        for (auto &resource : materialStructNamedResources)
        {
            if (resource.ssbo_name != ssbo_name)
                continue;

            ResetMaterialStructNamedAuthoringResource(resource);
            return;
        }
    }

    void PrimitiveComponent::ClearMaterialStructResource(hgl::graph::mtl::DataSlot slot)
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialStructResources.size())
            return;

        ResetMaterialStructAuthoringResource(materialStructResources[index]);
    }

    void PrimitiveComponent::ClearMaterialAuthoringResources()
    {
        for (auto &resource : materialTextureResources)
            ResetMaterialTextureAuthoringResource(resource);

        for (auto &resource : materialStructResources)
            ResetMaterialStructAuthoringResource(resource);

        for (auto &resource : materialStructNamedResources)
            ResetMaterialStructNamedAuthoringResource(resource);
        materialStructNamedResources.clear();
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
        if (HasMaterialRecipe())
            return nullptr;

        // Return override material if set, otherwise primitive's material
        if (overrideMaterial)
            return overrideMaterial;

        if (!primitive)
            return nullptr;

        return primitive->GetMaterialInstance();
    }

    hgl::graph::DescriptorBindingSet* PrimitiveComponent::GetInternalDescriptorBindingSet() const
    {
        if (HasMaterialRecipe())
            return nullptr;

        if (descriptorBindingSet)
            return descriptorBindingSet;

        if (!primitive)
            return nullptr;

        return primitive->GetDescriptorBindingSet();
    }

    hgl::graph::MaterialProgram* PrimitiveComponent::GetMaterialProgram() const
    {
        if (HasMaterialRecipe())
            return nullptr;

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
        if (primitive)
        {
            const auto& bv = primitive->GetBoundingVolumes();
            outAABB = bv.aabb;
            return true;
        }

        if (!primitiveAsset || !primitiveAsset->GetGeometry())
            return false;

        const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
        outAABB = bv.aabb;
        return true;
    }

    bool PrimitiveComponent::CanRender() const
    {
        return (primitive != nullptr || primitiveAsset != nullptr) && IsVisible();
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
        primitiveAsset = nullptr;
        primitiveVariantIndex = 0;
        runtimePrimitiveFromAsset = false;
        overrideMaterial = nullptr;
        descriptorBindingSet = nullptr;
        overridePipeline = nullptr;
        ResetMaterialRecipe(materialRecipe);
        hasMaterialRecipe = false;
        ClearMaterialAuthoringResources();
        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;
        hasPendingPipelinePreset = false;
    }

    void PrimitiveComponent::SetInternalDescriptorBindingSet(hgl::graph::DescriptorBindingSet* set)
    {
        if (HasMaterialRecipe() && set)
        {
            GLogWarning("[PrimitiveComponent] Ignore SetInternalDescriptorBindingSet while recipe runtime is enabled.");
            return;
        }

        if (descriptorBindingSet != set)
        {
            InvalidateResolvedRuntimePipeline();
        }

        descriptorBindingSet = set;
    }
}//namespace hgl::ecs
