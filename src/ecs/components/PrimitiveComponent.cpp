#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/vk/VKShaderProgram.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    namespace
    {
        void ResetMaterialRecipe(hgl::graph::mtl::MaterialRecipe &recipe)
        {
            recipe.recipe_name.clear();
            recipe.mtl_def_id.clear();
            recipe.domain.clear();
            recipe.vertex_node_config = hgl::graph::mtl::MakeDefault3DNodeConfig();
            recipe.material_lod = 0;
            recipe.render_state_overrides.has_double_sided = true;
            recipe.render_state_overrides.double_sided = false;
            recipe.render_state_overrides.has_alpha_test = true;
            recipe.render_state_overrides.alpha_test = false;
            recipe.render_state_overrides.has_alpha_cutoff = true;
            recipe.render_state_overrides.alpha_cutoff = 0.5f;
            recipe.render_state_overrides.has_dither = true;
            recipe.render_state_overrides.dither = false;
            recipe.render_state_overrides.has_pipeline_config = true;
            recipe.render_state_overrides.pipeline_config = hgl::graph::mtl::MaterialPipelineConfig{};
            recipe.textures.clear();
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

        void ResetMaterialDataSlotAuthoringResource(PrimitiveComponent::MaterialDataSlotAuthoringResource &resource)
        {
            resource.data_slot_name.clear();
            resource.data_slot = hgl::graph::mtl::DefaultMaterialDataSlot;
            resource.ssbo_type = hgl::graph::mtl::SSBOType::UserDefined;
            resource.ssbo_id = 0;
            resource.buffer = nullptr;
            resource.element_capacity = 0;
            resource.byte_stride = 0;
            resource.data_index = 0;
            resource.use_data_index = false;
            resource.shared_across_instances = false;
            resource.authored = false;
        }

        void UpsertRecipeTextureBinding(hgl::graph::mtl::MaterialRecipe &recipe,
                                        const std::string &slot_name,
                                        const std::string &resource_id,
                                        const bool required,
                                        const uint32_t direct_value = 0,
                                        const bool use_direct_value = false)
        {
            for (auto &binding : recipe.textures)
            {
                if (binding.slot_name != slot_name)
                    continue;

                binding.resource_id = resource_id;
                binding.direct_value = direct_value;
                binding.use_direct_value = use_direct_value;
                binding.required = required;
                return;
            }

            hgl::graph::mtl::RecipeTextureBinding binding{};
            binding.slot_name = slot_name;
            binding.resource_id = resource_id;
            binding.direct_value = direct_value;
            binding.use_direct_value = use_direct_value;
            binding.required = required;
            recipe.textures.emplace_back(std::move(binding));
        }

        void NormalizeRecipeWithBaseMaterialInfo(hgl::graph::mtl::MaterialRecipe &recipe)
        {
            hgl::graph::mtl::NormalizeRecipe(recipe);
        }
    }

    bool PrimitiveComponent::EnsureRuntimeGeometryBinding(hgl::graph::ShaderProgram *material)
    {
        if (!primitiveAsset || !material)
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: primitiveAsset=%p material=%p",
                      primitiveAsset,
                      material);
            return false;
        }

        auto *geometry = primitiveAsset->GetGeometry();
        if (!geometry)
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: geometry null asset=%p material=%s",
                      primitiveAsset,
                      material->GetName().c_str());
            return false;
        }

        if (!runtime_draw_range)
            runtime_draw_range = new hgl::graph::GeometryDrawRange();

        if (!runtime_draw_range)
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: alloc GeometryDrawRange failed material=%s",
                      material->GetName().c_str());
            return false;
        }

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
            {
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: CreateVIL/default fallback both null material=%s geom_attr_count=%u",
                          material->GetName().c_str(),
                          geometry->GetGeometryVertexFormat().GetCount());
                return false;
            }

            const uint32_t input_count = runtime_vil->GetVertexAttribCount();
            if (geometry->GetVABCount() < input_count)
            {
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: geometry VAB count(%u) < VIL input count(%u), material=%s",
                          geometry->GetVABCount(),
                          input_count,
                          material->GetName().c_str());
                return false;
            }

            const graph::GeometryVertexFormatMatch match_result =
                graph::MatchGeometryVertexFormat(geometry->GetGeometryVertexFormat(),
                                                 runtime_vil->GetVIFList(),
                                                 input_count);
            if (!match_result.IsDirectBindSatisfied())
            {
                const graph::GeometryVertexFailureSummary failure_summary = match_result.BuildFailureSummary();
                const graph::GeometryVertexAttributeMatch *first_issue = failure_summary.first_failure;
                if (first_issue)
                {
                    GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: vertex format mismatch material=%s semantic=%s kind=%s geom_fmt=%s mtl_fmt=%s",
                              material->GetName().c_str(),
                              graph::GetVertexSemanticName(first_issue->semantic),
                              graph::GetGeometryVertexMatchKindName(first_issue->kind),
                              graph::GetVulkanFormatName(first_issue->geometry_format),
                              graph::GetVulkanFormatName(first_issue->material_format));
                }
                else
                {
                    GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: vertex format mismatch material=%s",
                              material->GetName().c_str());
                }
                return false;
            }

            SAFE_CLEAR(runtime_data_buffer);

            runtime_data_buffer = new hgl::graph::GeometryDataBuffer(input_count,
                                                                     geometry->GetIBO(),
                                                                     geometry->GetVDM());
            if (!runtime_data_buffer)
            {
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: alloc GeometryDataBuffer failed material=%s vif_count=%u",
                          material->GetName().c_str(),
                          runtime_vil->GetVertexAttribCount());
                return false;
            }

            runtime_geometry = geometry;
            runtime_material = material;
        }

        if (!runtime_data_buffer->Update(geometry, runtime_vil->GetVIFList(), runtime_vil->GetVertexAttribCount()))
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: GeometryDataBuffer::Update failed material=%s vif_count=%u vif_list=%p",
                      material->GetName().c_str(),
                      runtime_vil->GetVertexAttribCount(),
                      runtime_vil->GetVIFList());
            return false;
        }

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

        ++material_authored_generation;
    }

    void PrimitiveComponent::SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        InvalidateResolvedRuntimePipeline();
        materialRecipeOverride = recipe;
        hasMaterialRecipeOverride = true;
        ++material_authored_generation;
    }

    const hgl::graph::mtl::MaterialRecipe *PrimitiveComponent::GetMaterialRecipeOverride() const
    {
        if (hasMaterialRecipeOverride)
            return &materialRecipeOverride;

        return nullptr;
    }

    const hgl::graph::mtl::MaterialRecipe *PrimitiveComponent::GetAssetMaterialRecipe() const
    {
        if (!primitiveAsset)
            return nullptr;

        if (const auto *variant =
                primitiveAsset->FindVariantByPurpose(
                    primitiveVariantPurpose,
                    primitiveVariantIndex))
            return variant->material_recipe;

        return primitiveAsset->GetMaterialRecipe();
    }

    bool PrimitiveComponent::BuildResolvedAuthoringMaterialRecipe(hgl::graph::mtl::MaterialRecipe &out_recipe,
                                                                  const hgl::graph::ShaderProgram *material_program) const
    {
        const auto *asset_recipe = GetAssetMaterialRecipe();
        const auto *override_recipe = GetMaterialRecipeOverride();

        if (asset_recipe)
            out_recipe = *asset_recipe;
        else
        if (override_recipe)
            out_recipe = *override_recipe;
        else
            return false;

        if (asset_recipe && override_recipe)
            out_recipe = *override_recipe;

        for (size_t i = 0; i < static_cast<size_t>(hgl::graph::mtl::TextureSlot::RANGE_SIZE); ++i)
        {
            const auto slot = static_cast<hgl::graph::mtl::TextureSlot>(i);
            const auto *resource = GetMaterialTextureResource(slot);
            if (!resource)
                continue;

            if (resource->use_direct_value)
            {
                UpsertRecipeTextureBinding(out_recipe,
                                           hgl::graph::mtl::GetTextureSlotName(slot),
                                           std::string(),
                                           resource->required,
                                           resource->direct_value,
                                           true);
                continue;
            }

            const std::string resource_id = resource->resource_id.empty()
                                          ? BuildTextureResourceId(resource->texture)
                                          : resource->resource_id;
            if (resource_id.empty())
                continue;

            UpsertRecipeTextureBinding(out_recipe, hgl::graph::mtl::GetTextureSlotName(slot), resource_id, resource->required);
        }

        for (const auto &resource : materialDataSlotResources)
        {
            if (!resource.authored)
                continue;

            hgl::graph::mtl::SSBOType ssbo_type =
                hgl::graph::mtl::ResolveRecipeSSBOType(
                    out_recipe,
                    resource.data_slot_name.c_str(),
                    resource.data_slot,
                    resource.ssbo_type);

            if (material_program)
            {
                for (const auto &req : material_program->GetShaderResourceSchema().resources)
                {
                    if (req.semantic != hgl::graph::mtl::DescriptorSemantic::MaterialDataSlotData
                     || req.data_slot != resource.data_slot
                     || resource.data_slot_name != req.name)
                        continue;

                    if (ssbo_type != hgl::graph::mtl::SSBOType::UserDefined
                     && ssbo_type != req.ssbo_type)
                        return false;

                    ssbo_type = req.ssbo_type;
                    break;
                }
            }

            if (!hgl::graph::mtl::IsMaterialSSBOType(ssbo_type)
             || !hgl::graph::mtl::UpsertRecipeSSBOAssetBinding(
                    out_recipe,
                    resource.data_slot_name,
                    ssbo_type,
                    resource.ssbo_id,
                    resource.data_slot,
                    resource.data_index,
                    resource.use_data_index,
                    resource.shared_across_instances))
                return false;
        }

        NormalizeRecipeWithBaseMaterialInfo(out_recipe);
        return true;
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
        ++material_authored_generation;
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
        ++material_authored_generation;
    }

    const PrimitiveComponent::MaterialTextureAuthoringResource *PrimitiveComponent::GetMaterialTextureResource(hgl::graph::mtl::TextureSlot slot) const
    {
        const size_t index = static_cast<size_t>(slot);
        if (index >= materialTextureResources.size())
            return nullptr;

        const auto &resource = materialTextureResources[index];
        return (resource.use_direct_value || (resource.texture && resource.sampler)) ? &resource : nullptr;
    }

    void PrimitiveComponent::SetMaterialDataSlotResource(const MaterialDataSlotAuthoringResource &resource)
    {
        if (!hgl::graph::mtl::IsValidMaterialDataSlotName(resource.data_slot_name))
            return;

        if (resource.ssbo_id == 0)
        {
            ClearMaterialDataSlotResource(resource.data_slot_name, resource.data_slot);
            return;
        }

        for (auto &existing : materialDataSlotResources)
        {
            if (existing.data_slot_name != resource.data_slot_name
             || existing.data_slot != resource.data_slot)
                continue;

            existing = resource;
            existing.authored = true;
            ++material_authored_generation;
            return;
        }

        MaterialDataSlotAuthoringResource authored_resource = resource;
        authored_resource.authored = true;
        materialDataSlotResources.emplace_back(std::move(authored_resource));
        ++material_authored_generation;
    }

    const PrimitiveComponent::MaterialDataSlotAuthoringResource *
        PrimitiveComponent::GetMaterialDataSlotResource(
            const std::string &data_slot_name,
            const uint32_t data_slot) const
    {
        if (!hgl::graph::mtl::IsValidMaterialDataSlotName(data_slot_name))
            return nullptr;

        for (const auto &resource : materialDataSlotResources)
        {
            if (resource.authored
             && resource.data_slot_name == data_slot_name
             && resource.data_slot == data_slot)
                return &resource;
        }

        return nullptr;
    }

    void PrimitiveComponent::ClearMaterialDataSlotResource(
        const std::string &data_slot_name,
        const uint32_t data_slot)
    {
        if (!hgl::graph::mtl::IsValidMaterialDataSlotName(data_slot_name))
            return;

        for (auto &resource : materialDataSlotResources)
        {
            if (resource.data_slot_name != data_slot_name
             || resource.data_slot != data_slot)
                continue;

            ResetMaterialDataSlotAuthoringResource(resource);
            ++material_authored_generation;
            return;
        }
    }

    void PrimitiveComponent::ClearMaterialAuthoringResources()
    {
        for (auto &resource : materialTextureResources)
            ResetMaterialTextureAuthoringResource(resource);

        for (auto &resource : materialDataSlotResources)
            ResetMaterialDataSlotAuthoringResource(resource);
        materialDataSlotResources.clear();
    }

    void PrimitiveComponent::InvalidateResolvedRuntimePipeline()
    {
        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;
    }

    hgl::graph::ShaderProgram* PrimitiveComponent::GetShaderProgram() const
    {
        // Recipe runtime resolves program via MaterialComponent; non-recipe items have no program.
        return nullptr;
    }

    hgl::graph::Pipeline* PrimitiveComponent::GetPipeline() const
    {
        if (overridePipeline)
            return overridePipeline;

        // Return the pipeline resolved during collect/prepare phases.
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
        ResetMaterialRecipe(materialRecipeOverride);
        hasMaterialRecipeOverride = false;
        ClearMaterialAuthoringResources();
        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;
        ++material_authored_generation;
    }
}//namespace hgl::ecs
