#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/mesh/GeometryDrawRange.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialLibrary.h>
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

        void UpsertRecipeTextureBinding(hgl::graph::mtl::MaterialRecipe &recipe,
                                        hgl::graph::mtl::TextureSlot slot,
                                        const std::string &resource_id,
                                        const bool required,
                                        const uint32_t direct_value = 0,
                                        const bool use_direct_value = false)
        {
            for (auto &binding : recipe.textures)
            {
                if (binding.slot != slot)
                    continue;

                binding.resource_id = resource_id;
                binding.direct_value = direct_value;
                binding.use_direct_value = use_direct_value;
                binding.required = required;
                return;
            }

            hgl::graph::mtl::RecipeTextureBinding binding{};
            binding.slot = slot;
            binding.resource_id = resource_id;
            binding.direct_value = direct_value;
            binding.use_direct_value = use_direct_value;
            binding.required = required;
            recipe.textures.emplace_back(std::move(binding));
        }

        void UpsertRecipeStructBinding(hgl::graph::mtl::MaterialRecipe &recipe,
                                       const uint32_t ssbo_slot,
                                       hgl::graph::mtl::SSBOType ssbo_type,
                                       const uint32_t ssbo_id,
                                       const uint32_t ssbo_element_index,
                                       const bool use_ssbo_element_index,
                                       const bool shared_across_instances)
        {
            for (auto &binding : recipe.structs)
            {
                if (binding.ssbo_slot != ssbo_slot || binding.ssbo_type != ssbo_type)
                    continue;

                binding.ssbo_type = ssbo_type;
                binding.ssbo_id = ssbo_id;
                binding.ssbo_element_index = ssbo_element_index;
                binding.use_ssbo_element_index = use_ssbo_element_index;
                binding.shared_across_instances = shared_across_instances;
                return;
            }

            hgl::graph::mtl::RecipeStructBinding binding{};
            binding.ssbo_slot = ssbo_slot;
            binding.ssbo_type = ssbo_type;
            binding.ssbo_id = ssbo_id;
            binding.ssbo_element_index = ssbo_element_index;
            binding.use_ssbo_element_index = use_ssbo_element_index;
            binding.shared_across_instances = shared_across_instances;
            recipe.structs.emplace_back(std::move(binding));
        }

        void NormalizeRecipeWithBaseMaterialInfo(hgl::graph::mtl::MaterialRecipe &recipe)
        {
            if (recipe.mtl_def_id.empty())
                return;

            hgl::graph::mtl::MaterialDefinition bmi{};
            if (hgl::graph::mtl::TryGetMaterialDefinitionByID(recipe.mtl_def_id, bmi))
                hgl::graph::mtl::ApplyBaseMaterialInfoDefaults(recipe, bmi, false);

            if (recipe.ssbo_assets.empty())
            {
                for (const auto &binding : recipe.structs)
                    hgl::graph::mtl::UpsertRecipeSSBOAssetBinding(recipe, "mtl", hgl::graph::mtl::SSBOBinding{binding.ssbo_type, binding.ssbo_id});
            }
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
    }

    void PrimitiveComponent::SetMaterialRecipe(const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        InvalidateResolvedRuntimePipeline();
        materialRecipeOverride = recipe;
        hasMaterialRecipeOverride = true;
    }

    const hgl::graph::mtl::MaterialRecipe *PrimitiveComponent::GetMaterialRecipe() const
    {
        return GetMaterialRecipeOverride();
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

        if (const auto *variant = primitiveAsset->GetVariant(primitiveVariantIndex))
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
                                           slot,
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

            UpsertRecipeTextureBinding(out_recipe, slot, resource_id, resource->required);
        }

        for (size_t i = 0; i < materialSSBOResources.size(); ++i)
        {
            const auto *resource = GetMaterialSSBOResourceBySlot(static_cast<uint32_t>(i));
            if (!resource)
                continue;

            UpsertRecipeStructBinding(out_recipe,
                                      static_cast<uint32_t>(i),
                                      resource->ssbo_type,
                                      resource->ssbo_id,
                                      resource->ssbo_element_index,
                                      resource->use_ssbo_element_index,
                                      resource->shared_across_instances);
        }

        if (material_program)
        {
            for (const auto &req : material_program->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != hgl::graph::mtl::DescriptorSemantic::MaterialSSBOSlotData || !req.name || !*req.name)
                    continue;

                const auto *named_resource = GetMaterialSSBOResource(std::string(req.name));
                if (!named_resource)
                    continue;

                hgl::graph::mtl::UpsertRecipeSSBOAssetBinding(out_recipe,
                                                              std::string(req.name),
                                                              req.ssbo_type,
                                                              named_resource->ssbo_id);

                if (GetMaterialSSBOResourceBySlot(req.ssbo_slot))
                    continue;

                UpsertRecipeStructBinding(out_recipe,
                                          req.ssbo_slot,
                                          req.ssbo_type,
                                          named_resource->ssbo_id,
                                          named_resource->ssbo_element_index,
                                          named_resource->use_ssbo_element_index,
                                          named_resource->shared_across_instances);
            }
        }

        NormalizeRecipeWithBaseMaterialInfo(out_recipe);
        return true;
    }

    void PrimitiveComponent::ClearMaterialRecipe()
    {
        InvalidateResolvedRuntimePipeline();
        ResetMaterialRecipe(materialRecipeOverride);
        hasMaterialRecipeOverride = false;
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
        ResetMaterialRecipe(materialRecipeOverride);
        hasMaterialRecipeOverride = false;
        ClearMaterialAuthoringResources();
        resolvedRuntimePipeline = nullptr;
        resolvedRuntimeRenderPass = nullptr;
        hasPendingPipelinePreset = false;
    }
}//namespace hgl::ecs
