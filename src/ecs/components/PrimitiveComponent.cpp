#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderItemDataStorage.h>
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
            recipe.vertex_node_config = hgl::graph::mtl::MakeDefault3DNodeConfig();
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
            recipe.material_ssbo_binding = {};
        }

        void ResetMaterialDataAuthoringResource(
            PrimitiveComponent::MaterialDataAuthoringResource &resource)
        {
            resource.ssbo_type = hgl::graph::GlobalSSBOType::PBRSurface;
            resource.ssbo_id = 0;
            resource.buffer = nullptr;
            resource.element_capacity = 0;
            resource.byte_stride = 0;
            resource.data_index = uint32_t(-1);
            resource.authored = false;
        }

        bool ResolveTextureAuthoringDefinition(
            const PrimitiveComponent &component,
            hgl::graph::mtl::MaterialDefinition &out_definition)
        {
            const hgl::graph::mtl::MaterialRecipe *recipe =
                component.GetMaterialRecipeOverride();
            if (!recipe)
                recipe = component.GetAssetMaterialRecipe();

            return recipe
                && !recipe->mtl_def_id.empty()
                && hgl::graph::mtl::TryGetMaterialDefinitionByID(
                    recipe->mtl_def_id,
                    out_definition);
        }

        bool UpsertMaterialTextureAuthoringResource(
            hgl::UnorderedMap<hgl::AnsiString,
                PrimitiveComponent::MaterialTextureAuthoringResource> &resources,
            const std::string &name,
            const PrimitiveComponent::MaterialTextureAuthoringResource &resource)
        {
            const hgl::AnsiString key(name.c_str());
            if (resources.GetValuePointer(key))
                return resources.Change(key, resource);
            return resources.Add(key, resource);
        }

        bool AppendTextureBinding(
            hgl::graph::mtl::MaterialRecipe &recipe,
            const std::string &name,
            const PrimitiveComponent::MaterialTextureAuthoringResource &resource)
        {
            if (!resource.texture || !resource.sampler)
                return true;

            const std::string resource_id = resource.resource_id.empty()
                ? BuildTextureResourceId(resource.texture)
                : resource.resource_id;
            if (resource_id.empty())
                return false;

            return hgl::graph::mtl::UpsertRecipeTextureBinding(
                recipe,
                name,
                resource_id,
                resource.required,
                resource.array_layer);
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
         || (runtime_material != material);

        if (needs_rebuild)
        {
            // 顶点输入统一为 SSBO：无 VIL attribute 布局，顶点数据槽位
            // 直接按 Geometry 语义列表填充（GeometryDataBuffer::Update）
            const uint32_t input_count = geometry->GetGeometryVertexFormat().GetCount();

            if (geometry->GetVABCount() < input_count)
            {
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: geometry VAB count(%u) < semantic count(%u), material=%s",
                          geometry->GetVABCount(),
                          input_count,
                          material->GetName().c_str());
                return false;
            }

            SAFE_CLEAR(runtime_data_buffer);

            runtime_data_buffer = new hgl::graph::GeometryDataBuffer(input_count,
                                                                     geometry->GetIBO(),
                                                                     geometry->GetVDM());
            if (!runtime_data_buffer)
            {
                GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: alloc GeometryDataBuffer failed material=%s attr_count=%u",
                          material->GetName().c_str(),
                          input_count);
                return false;
            }

            runtime_geometry = geometry;
            runtime_material = material;
        }

        if (!runtime_data_buffer->Update(geometry))
        {
            GLogError("[PrimitiveComponent] EnsureRuntimeGeometryBinding failed: GeometryDataBuffer::Update failed material=%s",
                      material->GetName().c_str());
            return false;
        }

        runtime_draw_range->Set(geometry);
        return true;
    }

    void PrimitiveComponent::ClearRuntimeGeometryBinding()
    {
        SAFE_CLEAR(runtime_data_buffer);
        SAFE_CLEAR(runtime_draw_range);
        runtime_geometry = nullptr;
        runtime_material = nullptr;
    }

    const hgl::graph::GeometryDataBuffer *PrimitiveComponent::GetRuntimeGeometryDataBuffer() const
    {
        return runtime_data_buffer;
    }

    const hgl::graph::GeometryDrawRange *PrimitiveComponent::GetRuntimeGeometryDrawRange() const
    {
        return runtime_draw_range;
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
        (void)material_program;

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

        hgl::graph::mtl::MaterialDefinition definition{};
        const bool has_definition =
            !out_recipe.mtl_def_id.empty()
         && hgl::graph::mtl::TryGetMaterialDefinitionByID(
                out_recipe.mtl_def_id,
                definition);

        for (const auto &[named_key, resource] : namedMaterialTextureResources)
        {
            const std::string name = named_key.c_str();
            if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
                return false;

            const int declaration_index = has_definition
                ? hgl::graph::mtl::FindMaterialTextureDeclaration(
                    definition,
                    name)
                : -1;
            if (!has_definition || declaration_index < 0)
                return false;

            if (declaration_index >= 0)
            {
                const auto &declaration =
                    definition.texture_declarations[
                        static_cast<size_t>(declaration_index)];
                const bool declaration_uses_array =
                    hgl::graph::mtl::IsMaterialTextureArraySampler(
                        declaration.sampler_type);
                const bool authoring_uses_array =
                    resource.kind
                    == MaterialTextureResourceKind::Texture2DArray;
                if ((!declaration_uses_array && authoring_uses_array)
                 || (!authoring_uses_array && resource.array_layer != 0))
                    return false;
            }
        }

        if (has_definition)
        {
            for (const auto &declaration : definition.texture_declarations)
            {
                const hgl::AnsiString key(declaration.name.c_str());
                const auto *resource =
                    namedMaterialTextureResources.GetValuePointer(key);
                if (resource
                 && !AppendTextureBinding(
                        out_recipe,
                        declaration.name,
                        *resource))
                    return false;

                bool has_recipe_binding = false;
                for (const auto &binding : out_recipe.textures)
                {
                    if (binding.texture_name == declaration.name)
                    {
                        has_recipe_binding = true;
                        if (!hgl::graph::mtl::IsMaterialTextureArraySampler(
                                declaration.sampler_type)
                         && binding.array_layer != 0)
                            return false;
                        break;
                    }
                }
                if (!has_recipe_binding
                 && !hgl::graph::mtl::UpsertRecipeTextureBinding(
                        out_recipe,
                        declaration.name,
                        std::string(),
                        declaration.required))
                    return false;
            }
        }

        const MaterialDataAuthoringResource &resource =
            materialDataResource;
        if (resource.authored)
        {
            hgl::graph::GlobalSSBOBinding material_ssbo_binding =
                resource.GetGlobalSSBOBinding();
            material_ssbo_binding.ssbo_type =
                hgl::graph::mtl::ResolveRecipeSSBOType(
                    out_recipe,
                    material_ssbo_binding.ssbo_type);

            if (!material_ssbo_binding.IsValid())
                return false;

            out_recipe.material_ssbo_binding = material_ssbo_binding;
        }

        // 组件边界规范化：写回 mtl_def_id 权威值与解析后的渲染状态，
        // 下游（acquire / 管线创建）不再重复。
        hgl::graph::mtl::NormalizeRecipe(out_recipe);
        return true;
    }

    bool PrimitiveComponent::SetMaterialTextureResource(
        const std::string &name,
        hgl::graph::Texture *texture,
        hgl::graph::Sampler *sampler,
        MaterialTextureResourceKind kind,
        const std::string &resource_id,
        const uint32_t array_layer,
        const bool required)
    {
        if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
        {
            GLogError(
                "[PrimitiveComponent] Texture authoring rejected invalid name=%s",
                name.c_str());
            return false;
        }
        if (!texture || !sampler)
        {
            GLogError(
                "[PrimitiveComponent] Texture authoring rejected null resource name=%s",
                name.c_str());
            return false;
        }

        hgl::graph::mtl::MaterialDefinition definition{};
        if (!ResolveTextureAuthoringDefinition(*this, definition))
        {
            GLogError(
                "[PrimitiveComponent] Texture authoring rejected unresolved material definition texture=%s",
                name.c_str());
            return false;
        }

        const int declaration_index =
            hgl::graph::mtl::FindMaterialTextureDeclaration(
                definition,
                name);
        if (declaration_index < 0)
        {
            GLogError(
                "[PrimitiveComponent] Texture authoring rejected undeclared texture=%s definition=%s",
                name.c_str(),
                definition.definition_id.c_str());
            return false;
        }
        const auto &declaration =
            definition.texture_declarations[
                static_cast<size_t>(declaration_index)];
        const bool declaration_uses_array =
            hgl::graph::mtl::IsMaterialTextureArraySampler(
                declaration.sampler_type);
        const bool authoring_uses_array =
            kind == MaterialTextureResourceKind::Texture2DArray;
        if ((!declaration_uses_array && authoring_uses_array)
         || (!authoring_uses_array && array_layer != 0))
        {
            GLogError(
                "[PrimitiveComponent] Texture authoring rejected incompatible texture type/layer texture=%s layer=%u",
                name.c_str(),
                array_layer);
            return false;
        }

        MaterialTextureAuthoringResource resource{};
        resource.resource_id =
            resource_id.empty() ? BuildTextureResourceId(texture) : resource_id;
        resource.texture = texture;
        resource.sampler = sampler;
        resource.kind = kind;
        resource.array_layer = array_layer;
        resource.required = required;
        if (!UpsertMaterialTextureAuthoringResource(
                namedMaterialTextureResources,
                name,
                resource))
            return false;
        ++material_authored_generation;
        return true;
    }

    bool PrimitiveComponent::SetMaterialTextureArrayLayer(
        const std::string &name,
        const uint32_t array_layer)
    {
        if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
        {
            GLogError(
                "[PrimitiveComponent] Texture layer rejected invalid name=%s",
                name.c_str());
            return false;
        }

        hgl::graph::mtl::MaterialDefinition definition{};
        if (!ResolveTextureAuthoringDefinition(*this, definition))
        {
            GLogError(
                "[PrimitiveComponent] Texture layer rejected unresolved material definition texture=%s",
                name.c_str());
            return false;
        }
        const int declaration_index =
            hgl::graph::mtl::FindMaterialTextureDeclaration(
                definition,
                name);
        if (declaration_index < 0
         || !hgl::graph::mtl::IsMaterialTextureArraySampler(
                definition.texture_declarations[
                    static_cast<size_t>(declaration_index)].sampler_type))
        {
            GLogError(
                "[PrimitiveComponent] Texture layer rejected non-array or undeclared texture=%s definition=%s",
                name.c_str(),
                definition.definition_id.c_str());
            return false;
        }

        const hgl::AnsiString key(name.c_str());
        if (auto *entry = namedMaterialTextureResources.GetValuePointer(key))
        {
            entry->array_layer = array_layer;
            ++material_authored_generation;
            return true;
        }
        GLogError(
            "[PrimitiveComponent] Texture layer rejected missing resource texture=%s",
            name.c_str());
        return false;
    }

    const PrimitiveComponent::MaterialTextureAuthoringResource *
        PrimitiveComponent::GetMaterialTextureResource(
            const std::string &name) const
    {
        if (!hgl::graph::mtl::IsValidMaterialTextureName(name))
            return nullptr;

        const hgl::AnsiString key(name.c_str());
        if (const auto *entry = namedMaterialTextureResources.GetValuePointer(key))
        {
            if (entry->texture && entry->sampler)
                return entry;
        }
        return nullptr;
    }

    void PrimitiveComponent::SetMaterialDataResource(
        const MaterialDataAuthoringResource &resource)
    {
        if (resource.ssbo_id == 0)
        {
            ClearMaterialDataResource();
            return;
        }

        if (!resource.GetGlobalSSBOBinding().IsValid())
        {
            GLogError(
                "[PrimitiveComponent] Material data resource rejected missing active row ID type=%s ssbo_id=%u data_index=%u",
                hgl::graph::GetGlobalSSBOTypeName(
                    resource.ssbo_type),
                resource.ssbo_id,
                resource.data_index);
            return;
        }

        materialDataResource = resource;
        materialDataResource.authored = true;
        ++material_authored_generation;
    }

    const PrimitiveComponent::MaterialDataAuthoringResource *
        PrimitiveComponent::GetMaterialDataResource() const
    {
        return materialDataResource.authored
            ? &materialDataResource : nullptr;
    }

    void PrimitiveComponent::ClearMaterialDataResource()
    {
        if (!materialDataResource.authored)
            return;

        ResetMaterialDataAuthoringResource(materialDataResource);
        ++material_authored_generation;
    }

    void PrimitiveComponent::ClearMaterialAuthoringResources()
    {
        namedMaterialTextureResources.Clear();
        ResetMaterialDataAuthoringResource(materialDataResource);
        ++material_authored_generation;
    }

    void PrimitiveComponent::InvalidateResolvedRuntimePipeline()
    {
        resolvedRuntimePipelineMap.Clear();
    }

    hgl::graph::ShaderProgram* PrimitiveComponent::GetShaderProgram() const
    {
        // Recipe runtime resolves program via MaterialComponent; non-recipe items have no program.
        return nullptr;
    }

    hgl::graph::Pipeline* PrimitiveComponent::GetPipelineForRenderPass(hgl::graph::RenderPass* render_pass) const
    {
        if (overridePipeline)
            return overridePipeline;

        // Return the pipeline resolved for THIS render pass during collect/prepare phases.
        // 注意：GetValuePointer 的 const 重载返回 const V*，此处需要可变指针语义，
        // 但 map 内容并不修改，用非 const this 的映射读取即可。
        auto *p = const_cast<PrimitiveComponent *>(this)->resolvedRuntimePipelineMap.GetValuePointer(render_pass);
        return p ? *p : nullptr;
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

    void PrimitiveComponent::EnsureRenderItemStorageAllocated()
    {
        if (!bound_render_item_storage && owner_context)
        {
            bound_render_item_storage = owner_context->GetRenderItemStorage();
        }
        if (bound_render_item_storage && render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
        {
            render_item_handle = bound_render_item_storage->Allocate(render_item_descriptor);
        }
    }

    graph::RenderItemHandle PrimitiveComponent::GetRenderItemHandle() const
    {
        if (render_item_handle == graph::INVALID_RENDER_ITEM_HANDLE)
        {
            const_cast<PrimitiveComponent*>(this)->EnsureRenderItemStorageAllocated();
        }
        return render_item_handle;
    }

    const graph::RenderItemDescriptor &PrimitiveComponent::GetRenderItemDescriptor() const
    {
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            if (const auto *desc = bound_render_item_storage->Get(render_item_handle))
                return *desc;
        }
        return render_item_descriptor;
    }

    void PrimitiveComponent::SetTransformID(uint32_t transform_id)
    {
        render_item_descriptor.transform_id = transform_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetTransformID(render_item_handle, transform_id);
        }
    }

    void PrimitiveComponent::SetGeometryID(uint32_t geometry_id)
    {
        render_item_descriptor.geometry_id = geometry_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetGeometryID(render_item_handle, geometry_id);
        }
    }

    void PrimitiveComponent::SetMaterialID(uint32_t material_id)
    {
        render_item_descriptor.material_id = material_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetMaterialID(render_item_handle, material_id);
        }
    }

    void PrimitiveComponent::SetTextureID(uint32_t texture_id)
    {
        render_item_descriptor.texture_id = texture_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->SetTextureID(render_item_handle, texture_id);
        }
    }

    void PrimitiveComponent::Set4ID(uint32_t transform_id, uint32_t geometry_id, uint32_t material_id, uint32_t texture_id)
    {
        render_item_descriptor.transform_id = transform_id;
        render_item_descriptor.geometry_id = geometry_id;
        render_item_descriptor.material_id = material_id;
        render_item_descriptor.texture_id = texture_id;
        EnsureRenderItemStorageAllocated();
        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->Set4ID(render_item_handle, transform_id, geometry_id, material_id, texture_id);
        }
    }


    void PrimitiveComponent::OnAttach()
    {
        RenderableComponent::OnAttach();
        EnsureRenderItemStorageAllocated();
    }

    void PrimitiveComponent::OnUpdate(float deltaTime)
    {
        RenderableComponent::OnUpdate(deltaTime);
        // Update logic if needed (e.g., animation updates)
    }

    void PrimitiveComponent::OnDetach()
    {
        RenderableComponent::OnDetach();

        if (bound_render_item_storage && render_item_handle != graph::INVALID_RENDER_ITEM_HANDLE)
        {
            bound_render_item_storage->Release(render_item_handle);
            render_item_handle = graph::INVALID_RENDER_ITEM_HANDLE;
            bound_render_item_storage = nullptr;
        }

        // Don't delete resources here; they are managed externally.
        primitiveAsset = nullptr;
        primitiveVariantIndex = 0;
        ClearRuntimeGeometryBinding();
        overridePipeline = nullptr;
        ResetMaterialRecipe(materialRecipeOverride);
        hasMaterialRecipeOverride = false;
        ClearMaterialAuthoringResources();
        resolvedRuntimePipelineMap.Clear();
        render_item_descriptor = {};
        ++material_authored_generation;
    }
}//namespace hgl::ecs
