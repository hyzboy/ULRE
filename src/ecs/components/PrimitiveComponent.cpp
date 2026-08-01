#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/vk/VKShaderProgram.h>
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
            recipe.mtl_def_id.clear();
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

        void ResetMaterialSSBOAuthoringResource(PrimitiveComponent::MaterialSSBOAuthoringResource &resource)
        {
            resource.ssbo_slot = hgl::graph::mtl::DefaultMaterialSSBOSlot;
            resource.ssbo_type = hgl::graph::mtl::SSBOType::UserDefined;
            resource.ssbo_id = 0;
            resource.buffer = nullptr;
            resource.element_capacity = 0;
            resource.byte_stride = 0;
            resource.ssbo_element_index = 0;
            resource.use_ssbo_element_index = false;
            resource.shared_across_instances = false;
            resource.authored = false;
        }

        void ResetMaterialSSBONamedAuthoringResource(PrimitiveComponent::MaterialSSBONamedAuthoringResource &resource)
        {
            resource.ssbo_name.clear();
            resource.ssbo_id = 0;
            resource.ssbo_element_index = 0;
            resource.use_ssbo_element_index = false;
            resource.shared_across_instances = false;
            resource.authored = false;
        }
    }

    bool PrimitiveComponent::EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material)
    {
        if (!primitiveAsset || !material)
            return false;

        auto *geometry = primitiveAsset->GetGeometry();
        if (!geometry)
            return false;

        const auto *vil = material->GetDefaultVIL();
        if (!vil)
            return false;

        if (!runtime_draw_range)
            runtime_draw_range = new hgl::graph::GeometryDrawRange();

        if (!runtime_draw_range)
            return false;

        const bool needs_rebuild =
            (!runtime_data_buffer)
         || (runtime_geometry != geometry)
         || (runtime_material != material)
         || (!runtime_vil);

        if (needs_rebuild)
        {
            if (runtime_vil_owned && runtime_material && runtime_vil)
                runtime_material->Release(const_cast<hgl::graph::VIL *>(runtime_vil));

            runtime_vil = material->CreateVIL(geometry->GetGeometryVertexFormat());
            runtime_vil_owned = (runtime_vil != nullptr);
            if (!runtime_vil)
            {
                runtime_vil = material->GetDefaultVIL();
                runtime_vil_owned = false;
            }
            if (!runtime_vil)
                return false;

            SAFE_CLEAR(runtime_data_buffer);

            runtime_data_buffer = new hgl::graph::GeometryDataBuffer(runtime_vil->GetVertexAttribCount(),
                                                                     geometry->GetIBO(),
                                                                     geometry->GetVDM());
            if (!runtime_data_buffer)
                return false;

            runtime_geometry = geometry;
            runtime_material = material;
        }

        if (!runtime_data_buffer->Update(geometry, runtime_vil->GetVIFList(), runtime_vil->GetVertexAttribCount()))
            return false;

        runtime_draw_range->Set(geometry);
        return true;
    }

    void PrimitiveComponent::ClearRuntimeGeometryBinding()
    {
        if (runtime_vil_owned && runtime_material && runtime_vil)
            runtime_material->Release(const_cast<hgl::graph::VIL *>(runtime_vil));

        SAFE_CLEAR(runtime_data_buffer);
        SAFE_CLEAR(runtime_draw_range);
        runtime_geometry = nullptr;
        runtime_material = nullptr;
        runtime_vil = nullptr;
        runtime_vil_owned = false;
    }

    const hgl::graph::GeometryDataBuffer *PrimitiveComponent::GetRuntimeGeometryDataBuffer() const
    {
        return runtime_data_buffer;
    }

    const hgl::graph::GeometryDrawRange *PrimitiveComponent::GetRuntimeGeometryDrawRange() const
    {
        return runtime_draw_range;
    }

    const hgl::graph::VertexInputLayout *PrimitiveComponent::GetRuntimeVIL() const
    {
        return runtime_vil;
    }

    void PrimitiveComponent::SetPrimitiveAsset(const hgl::graph::PrimitiveAsset *asset)
    {
        if (primitiveAsset != asset)
        {
            InvalidateResolvedRuntimePipeline();
            ClearRuntimeGeometryBinding();
        }

        primitiveAsset = asset;

        if (primitiveAsset && primitiveAsset->GetGeometry())
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

    void PrimitiveComponent::SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        InvalidateResolvedRuntimePipeline();
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

    void PrimitiveComponent::SetMaterialSSBOResource(hgl::graph::mtl::SSBOType ssbo_type,
                                                       uint32_t ssbo_id,
                                                       hgl::graph::DeviceBuffer *buffer,
                                                       uint32_t element_capacity,
                                                       uint32_t byte_stride,
                                                       uint32_t ssbo_element_index,
                                                       bool use_ssbo_element_index,
                                                       bool shared_across_instances)
    {
        SetMaterialSSBOResource(hgl::graph::mtl::DefaultMaterialSSBOSlot,
                                  ssbo_type,
                                  ssbo_id,
                                  buffer,
                                  element_capacity,
                                  byte_stride,
                                  ssbo_element_index,
                                  use_ssbo_element_index,
                                  shared_across_instances);
    }

    void PrimitiveComponent::SetMaterialSSBOResource(const uint32_t ssbo_slot,
                                                       hgl::graph::mtl::SSBOType ssbo_type,
                                                       uint32_t ssbo_id,
                                                       hgl::graph::DeviceBuffer *buffer,
                                                       uint32_t element_capacity,
                                                       uint32_t byte_stride,
                                                       uint32_t ssbo_element_index,
                                                       bool use_ssbo_element_index,
                                                       bool shared_across_instances)
    {
        if (!hgl::graph::mtl::IsMaterialSSBOType(ssbo_type))
            return;

        const size_t index = static_cast<size_t>(ssbo_slot);
        // Grow the vector on demand.
        if (index >= materialSSBOResources.size())
            materialSSBOResources.resize(index + 1);

        auto &resource = materialSSBOResources[index];
        const bool empty_authoring =
            !buffer &&
            ssbo_type == hgl::graph::mtl::SSBOType::UserDefined &&
            ssbo_id == 0 &&
            element_capacity == 0 &&
            byte_stride == 0 &&
            ssbo_element_index == 0 &&
            !use_ssbo_element_index &&
            !shared_across_instances;

        if (empty_authoring)
        {
            ResetMaterialSSBOAuthoringResource(resource);
            return;
        }

        resource.ssbo_slot = ssbo_slot;
        resource.ssbo_type = ssbo_type;
        resource.ssbo_id = ssbo_id;
        resource.buffer = buffer;
        resource.element_capacity = element_capacity;
        resource.byte_stride = byte_stride;
        resource.ssbo_element_index = ssbo_element_index;
        resource.use_ssbo_element_index = use_ssbo_element_index;
        resource.shared_across_instances = shared_across_instances;
        resource.authored = true;
    }

    const PrimitiveComponent::MaterialSSBOAuthoringResource *PrimitiveComponent::GetMaterialSSBOResource(hgl::graph::mtl::SSBOType ssbo_type) const
    {
        if (!hgl::graph::mtl::IsMaterialSSBOType(ssbo_type))
            return nullptr;

        for (const auto &resource : materialSSBOResources)
        {
            if (resource.authored && resource.ssbo_type == ssbo_type)
                return &resource;
        }

        return nullptr;
    }

    const PrimitiveComponent::MaterialSSBOAuthoringResource *PrimitiveComponent::GetMaterialSSBOResourceBySlot(const uint32_t ssbo_slot) const
    {
        const size_t index = static_cast<size_t>(ssbo_slot);
        if (index >= materialSSBOResources.size())
            return nullptr;

        const auto &resource = materialSSBOResources[index];
        return resource.authored ? &resource : nullptr;
    }

    void PrimitiveComponent::SetMaterialSSBOResource(const MaterialSSBONamedAuthoringResource &resource)
    {
        if (resource.ssbo_name.empty() || resource.ssbo_id == 0)
        {
            ClearMaterialSSBOResource(resource.ssbo_name);
            return;
        }

        for (auto &existing : materialSSBONamedResources)
        {
            if (existing.ssbo_name != resource.ssbo_name)
                continue;

            existing.ssbo_id = resource.ssbo_id;
            existing.ssbo_element_index = resource.ssbo_element_index;
            existing.use_ssbo_element_index = resource.use_ssbo_element_index;
            existing.shared_across_instances = resource.shared_across_instances;
            existing.authored = true;
            return;
        }

        MaterialSSBONamedAuthoringResource named = resource;
        named.authored = true;
        materialSSBONamedResources.emplace_back(std::move(named));
    }

    const PrimitiveComponent::MaterialSSBONamedAuthoringResource *PrimitiveComponent::GetMaterialSSBOResource(const std::string &ssbo_name) const
    {
        if (ssbo_name.empty())
            return nullptr;

        for (const auto &resource : materialSSBONamedResources)
        {
            if (resource.authored && resource.ssbo_name == ssbo_name)
                return &resource;
        }

        return nullptr;
    }

    void PrimitiveComponent::ClearMaterialSSBOResource(const std::string &ssbo_name)
    {
        if (ssbo_name.empty())
            return;

        for (auto &resource : materialSSBONamedResources)
        {
            if (resource.ssbo_name != ssbo_name)
                continue;

            ResetMaterialSSBONamedAuthoringResource(resource);
            return;
        }
    }

    void PrimitiveComponent::ClearMaterialSSBOResource(hgl::graph::mtl::SSBOType ssbo_type)
    {
        if (!hgl::graph::mtl::IsMaterialSSBOType(ssbo_type))
            return;

        for (auto &resource : materialSSBOResources)
        {
            if (resource.authored && resource.ssbo_type == ssbo_type)
                ResetMaterialSSBOAuthoringResource(resource);
        }
    }

    void PrimitiveComponent::ClearMaterialSSBOResourceBySlot(const uint32_t ssbo_slot)
    {
        const size_t index = static_cast<size_t>(ssbo_slot);
        if (index >= materialSSBOResources.size())
            return;

        ResetMaterialSSBOAuthoringResource(materialSSBOResources[index]);
    }

    void PrimitiveComponent::ClearMaterialAuthoringResources()
    {
        for (auto &resource : materialTextureResources)
            ResetMaterialTextureAuthoringResource(resource);

        for (auto &resource : materialSSBOResources)
            ResetMaterialSSBOAuthoringResource(resource);

        for (auto &resource : materialSSBONamedResources)
            ResetMaterialSSBONamedAuthoringResource(resource);
        materialSSBONamedResources.clear();
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

    hgl::graph::ShaderProgram* PrimitiveComponent::GetMaterialProgram() const
    {
        // Recipe runtime resolves program via MaterialComponent; non-recipe items have no program.
        return nullptr;
    }

    hgl::graph::Pipeline* PrimitiveComponent::GetPipeline() const
    {
        if (overridePipeline)
            return overridePipeline;

        // Return runtime-resolved pipeline if available (late-resolve path).
        return resolvedRuntimePipeline;
    }

    bool PrimitiveComponent::GetLocalAABB(hgl::math::AABB& outAABB) const
    {
        if (!primitiveAsset || !primitiveAsset->GetGeometry())
            return false;

        const auto &bv = primitiveAsset->GetGeometry()->GetBoundingVolumes();
        outAABB = bv.aabb;
        return true;
    }

    bool PrimitiveComponent::CanRender() const
    {
        return primitiveAsset != nullptr && IsVisible();
    }

    void PrimitiveComponent::Render(const glm::mat4& worldMatrix)
    {
        (void)worldMatrix;
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

        // Don't delete resources here; they are managed externally.
        primitiveAsset = nullptr;
        primitiveVariantIndex = 0;
        ClearRuntimeGeometryBinding();
        overridePipeline = nullptr;
        ResetMaterialRecipe(materialRecipe);
        hasMaterialRecipe = false;
        ClearMaterialAuthoringResources();
        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;
        hasPendingPipelinePreset = false;
    }
}//namespace hgl::ecs
