#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKRenderPass.h>
#include<glm/glm.hpp>
#include<cstring>

namespace hgl::ecs
{
    namespace
    {
        const char *GetPrimitiveOwnerName(const std::shared_ptr<PrimitiveComponent> &primitive_comp)
        {
            if (!primitive_comp)
                return "<null-primitive>";

            auto *owner = primitive_comp->GetOwner();
            if (!owner)
                return "<no-owner>";

            return owner->GetName().c_str();
        }


        std::string BuildTextureResourceId(graph::Texture *texture)
        {
            if (!texture)
                return {};

            return "texid:" + std::to_string(texture->GetID());
        }

        bool EnsureRuntimeGeometryFromAsset(ECSContext *world,
                                            const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                            const std::shared_ptr<MaterialComponent> &material_comp)
        {
            if (!world || !primitive_comp || !material_comp)
            {
                GLogError("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset precondition failed world=%p primitive=%p material_comp=%p",
                          world,
                          primitive_comp.get(),
                          material_comp.get());
                return false;
            }

            const auto *asset = primitive_comp->GetPrimitiveAsset();
            if (!asset)
                return true;

            auto *material = material_comp->program;
            if (!material)
            {
                GLogError("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed: material program null owner=%s valid=%d program_dirty=%d bindings_dirty=%d resources_dirty=%d",
                          GetPrimitiveOwnerName(primitive_comp),
                          material_comp->valid ? 1 : 0,
                          material_comp->program_dirty ? 1 : 0,
                          material_comp->bindings_dirty ? 1 : 0,
                          material_comp->resources_dirty ? 1 : 0);
                return false;
            }
            return primitive_comp->EnsureRuntimeGeometryBinding(material);
        }

        uint32_t ResolvePrimaryStructIndex(const graph::ShaderProgram *material,
                                           const graph::mtl::MaterializationSpec &spec,
                                           const uint32_t fallback_row)
        {
            if (material)
            {
                const auto &contract = material->GetMaterialResourceLayout();
                for (const auto &req : contract.requirements)
                {
                    if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                        continue;

                    for (const auto &ref : spec.struct_refs)
                    {
                        if (ref.data_slot == req.data_slot && ref.ssbo_type == req.ssbo_type)
                            return ref.data_index;
                    }
                }
            }

            if (!spec.struct_refs.empty())
                return spec.struct_refs.front().data_index;

            return fallback_row;
        }

        bool ResolveRecipeSSBOBindingId(const graph::mtl::MaterialRecipe &recipe,
                                        const graph::mtl::MaterialResourceRequirement &req,
                                        uint32_t &out_ssbo_id)
        {
            if (const auto *asset = graph::mtl::FindRecipeSSBOAssetBinding(
                    recipe, req.name, req.data_slot, req.ssbo_type))
            {
                out_ssbo_id = asset->ssbo_id;
                return true;
            }

            return false;
        }

        bool MaterialRequiresRecipeRuntimeRows(const graph::ShaderProgram *material)
        {
            if (!material)
                return false;

            for (const auto &req : material->GetMaterialResourceLayout().requirements)
            {
                switch (req.semantic)
                {
                    case graph::mtl::DescriptorSemantic::MaterialDataSlotData:
                    case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
                    case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
                        return true;
                    default:
                        break;
                }
            }

            return false;
        }

        bool BuildEffectiveMaterialRecipe(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                          const graph::ShaderProgram *material_program,
                                          graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!primitive_comp)
                return false;

            return primitive_comp->BuildResolvedAuthoringMaterialRecipe(out_recipe, material_program);
        }

        bool PrepareRecipeAuthoringResources(ECSContext *world,
                                             const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                             graph::ShaderProgram *material_program)
        {
            if (!world || !primitive_comp)
                return false;

            graph::mtl::MaterialRecipe effective_recipe{};
            if (!BuildEffectiveMaterialRecipe(primitive_comp, material_program, effective_recipe))
                return false;

            auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
            auto *render_context = world->GetRenderContext();
            auto *graphics_context = render_context ? render_context->GetGraphicsContext() : world->GetGraphicsContext();
            auto *domain_manager = graphics_context ? graphics_context->GetResourceDomainManager() : nullptr;
            auto *bindless_mgr = graphics_context ? graphics_context->GetManager<graph::BindlessTextureManager>() : nullptr;
            if (!rdbs || !domain_manager)
                return false;

            const char *prim_owner = GetPrimitiveOwnerName(primitive_comp);

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE); ++i)
            {
                const auto slot = static_cast<graph::mtl::TextureSlot>(i);
                const auto *resource = primitive_comp->GetMaterialTextureResource(slot);
                if (!resource)
                    continue;

                if (resource->use_direct_value)
                    continue;

                const std::string resource_id = resource->resource_id.empty()
                                              ? BuildTextureResourceId(resource->texture)
                                              : resource->resource_id;
                if (resource_id.empty() || !bindless_mgr)
                {
                    GLogError("[PrepareRecipe] %s slot=%zu: resource_id_empty=%d bindless_mgr_null=%d",
                              prim_owner, i, resource_id.empty()?1:0, !bindless_mgr?1:0);
                    return false;
                }

                uint32_t handle = 0;
                switch (resource->kind)
                {
                    case PrimitiveComponent::MaterialTextureResourceKind::Texture2D:
                        handle = rdbs->RegisterTexture2DResource(resource_id, resource->texture, resource->sampler, bindless_mgr);
                        break;
                    case PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray:
                        handle = rdbs->RegisterTexture2DArrayResource(resource_id, resource->texture, resource->sampler, bindless_mgr);
                        break;
                    default:
                        GLogWarning("[PrepareRecipe] %s slot=%zu unknown texture kind=%d", prim_owner, i, (int)resource->kind);
                        break;
                }

                if (handle == 0)
                {
                    GLogError("[PrepareRecipe] %s slot=%zu: RegisterTexture returned handle=0", prim_owner, i);
                    return false;
                }

                if (!material_program)
                    continue;

                for (const auto &req : material_program->GetMaterialResourceLayout().requirements)
                {
                    if (req.texture_slot != slot || !req.name || !*req.name)
                        continue;

                    switch (req.semantic)
                    {
                        case graph::mtl::DescriptorSemantic::MaterialTexture:
                            if (!rdbs->RegisterMaterialTexture(material_program, req.name, resource->texture))
                            {
                                GLogError("[PrepareRecipe] %s RegisterMaterialTexture FAILED descriptor=%s", prim_owner, req.name);
                                return false;
                            }
                            break;
                        case graph::mtl::DescriptorSemantic::MaterialSampler:
                            if (!rdbs->RegisterMaterialTextureSampler(material_program, req.name, resource->texture, resource->sampler))
                            {
                                GLogError("[PrepareRecipe] %s RegisterMaterialTextureSampler FAILED descriptor=%s", prim_owner, req.name);
                                return false;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }

            for (uint32_t i = 0; i < primitive_comp->GetMaterialDataSlotCount(); ++i)
            {
                const auto *resource = primitive_comp->GetMaterialDataSlotResourceAt(i);
                if (!resource)
                    continue;

                const graph::mtl::SSBOType ssbo_type =
                    graph::mtl::ResolveRecipeSSBOType(
                        effective_recipe,
                        resource->data_slot_name.c_str(),
                        resource->data_slot,
                        resource->ssbo_type);

                graph::mtl::SSBOType resolved_ssbo_type = ssbo_type;
                if (resolved_ssbo_type == graph::mtl::SSBOType::UserDefined && material_program)
                {
                    for (const auto &req : material_program->GetMaterialResourceLayout().requirements)
                    {
                        if (req.semantic == graph::mtl::DescriptorSemantic::MaterialDataSlotData
                         && req.name
                         && req.data_slot == resource->data_slot
                         && resource->data_slot_name == req.name)
                        {
                            resolved_ssbo_type = req.ssbo_type;
                            break;
                        }
                    }
                }

                if (!graph::mtl::IsMaterialSSBOType(resolved_ssbo_type))
                {
                    GLogError("[PrepareRecipe] %s: invalid SSBO type=%s name=%s slot=%u",
                              prim_owner,
                              graph::mtl::GetSSBOTypeName(resolved_ssbo_type),
                              resource->data_slot_name.c_str(),
                              resource->data_slot);
                    return false;
                }

                const graph::mtl::SSBOAddress address{resolved_ssbo_type, resource->ssbo_id, 0};
                if (resource->buffer)
                {
                    if (!rdbs->RegisterMaterialStructLayout(resolved_ssbo_type, resource->ssbo_id, resource->byte_stride))
                    {
                        GLogError("[PrepareRecipe] %s: SSBO layout registration failed type=%s id=%u name=%s slot=%u",
                                  prim_owner,
                                  graph::mtl::GetSSBOTypeName(resolved_ssbo_type),
                                  resource->ssbo_id,
                                  resource->data_slot_name.c_str(),
                                  resource->data_slot);
                        return false;
                    }

                    if (!domain_manager->RegisterBuffer(address, resource->buffer, resource->element_capacity))
                    {
                        GLogError("[PrepareRecipe] %s: SSBO buffer registration failed type=%s id=%u name=%s slot=%u",
                                  prim_owner,
                                  graph::mtl::GetSSBOTypeName(resolved_ssbo_type),
                                  resource->ssbo_id,
                                  resource->data_slot_name.c_str(),
                                  resource->data_slot);
                        return false;
                    }
                }
                else
                {
                    graph::ResourceDomainBinding binding{};
                    if (!domain_manager->TryGetBinding(address, binding) || !binding.buffer || binding.element_stride == 0)
                    {
                        GLogError("[PrepareRecipe] %s: SSBO binding not found type=%s id=%u name=%s slot=%u",
                                  prim_owner,
                                  graph::mtl::GetSSBOTypeName(resolved_ssbo_type),
                                  resource->ssbo_id,
                                  resource->data_slot_name.c_str(),
                                  resource->data_slot);
                        return false;
                    }

                    if (!rdbs->RegisterMaterialStructLayout(resolved_ssbo_type, resource->ssbo_id, binding.element_stride))
                    {
                        GLogError("[PrepareRecipe] %s: SSBO layout registration failed type=%s id=%u name=%s slot=%u",
                                  prim_owner,
                                  graph::mtl::GetSSBOTypeName(resolved_ssbo_type),
                                  resource->ssbo_id,
                                  resource->data_slot_name.c_str(),
                                  resource->data_slot);
                        return false;
                    }
                }
            }

            if (!material_program)
                return true;

            for (const auto &req : material_program->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                    continue;

                const auto *asset_binding = graph::mtl::FindRecipeSSBOAssetBinding(
                    effective_recipe, req.name, req.data_slot, req.ssbo_type);

                if (!asset_binding)
                    continue;

                const graph::mtl::SSBOAddress address{req.ssbo_type, asset_binding->ssbo_id, 0};
                graph::ResourceDomainBinding binding{};
                if (!domain_manager->TryGetBinding(address, binding) || !binding.buffer || binding.element_stride == 0)
                    return false;

                if (!rdbs->RegisterMaterialStructLayout(req.ssbo_type, asset_binding->ssbo_id, binding.element_stride))
                    return false;
            }

            return true;
        }

        void InvalidateRecipeRuntime(const std::shared_ptr<MaterialComponent> &material_comp,
                                     const bool clear_program)
        {
            if (!material_comp)
                return;

            material_comp->ClearMaterializationInstanceData();
            material_comp->bindings_dirty = true;
            material_comp->resources_dirty = true;
            material_comp->valid = false;

            if (clear_program)
            {
                material_comp->program = nullptr;
                material_comp->program_dirty = true;
            }
        }
    }

    RenderPrimitiveCollectSystem::RenderPrimitiveCollectSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetSystemType(SystemType::RenderCollect);
        SetExecutionOrder(ExecutionPhase::RenderCollect);
        SetRenderElementType("Primitive");

        // Declare dependencies
        AddDependency<TransformSystem>(); // Needs world transforms
        AddDependency<CameraSystem>();    // Needs camera info
    }

    bool RenderPrimitiveCollectSystem::ResolveMaterialProgramForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                                          const std::shared_ptr<MaterialComponent> &material_comp)
    {
        if (!world || !primitive_comp || !material_comp)
            return false;

        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildEffectiveMaterialRecipe(primitive_comp, nullptr, effective_recipe))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] BuildEffectiveMaterialRecipe failed for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(effective_recipe);
        auto *graphics = world->GetGraphicsContext();
        if (!graphics)
        {
            auto *render_context = world->GetRenderContext();
            graphics = render_context ? render_context->GetGraphicsContext() : nullptr;
        }
        if (!graphics)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialProgram failed: graphics context null for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        auto *material_manager = graphics->GetMaterialManager();
        if (!material_manager)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialProgram failed: material manager null for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
        const graph::GeometryVertexFormat *geometry_vertex_format = nullptr;
        if (const auto *asset = primitive_comp->GetPrimitiveAsset())
        {
            if (auto *asset_geometry = asset->GetGeometry())
                geometry_vertex_format = &asset_geometry->GetGeometryVertexFormat();
            primitive_type = asset->GetPrimitiveType();
        }

        const uint64_t build_context_hash =
            graph::mtl::HashMaterialProgramBuildContext(
                primitive_type,
                geometry_vertex_format,
                graphics->GetPhysicalDeviceProfile());
        if (material_comp->recipe_hash != recipe_hash
         || material_comp->program_build_context_hash
                != build_context_hash)
        {
            material_comp->program_dirty = true;
            InvalidateRecipeRuntime(material_comp, false);
        }

        if (!material_comp->program_dirty && material_comp->program)
            return true;

        // 统一 BMI 入口：由 AcquireShaderProgram 内部处理 2D/3D/Text/Sky 分支，
        // ECS 不再持有材质 config 细节知识。
        graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        switch (primitive_comp->GetPrimitiveVariantPurpose())
        {
        case graph::PrimitiveVariantPurpose::DepthOnly:
            mtl_request.override_shader_program_purpose = true;
            mtl_request.shader_program_purpose =
                graph::mtl::ShaderProgramPurpose::DepthOnly;
            break;
        case graph::PrimitiveVariantPurpose::ShadowCaster:
            mtl_request.override_shader_program_purpose = true;
            mtl_request.shader_program_purpose =
                graph::mtl::ShaderProgramPurpose::ShadowDepth;
            break;
        default:
            break;
        }
        graph::ShaderProgram *resolved_program =
            material_manager->AcquireShaderProgram(mtl_request);

        if (!resolved_program)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] AcquireShaderProgram failed for %s recipe=%s mtl_def_id=%s",
                        GetPrimitiveOwnerName(primitive_comp),
                        effective_recipe.recipe_name.c_str(),
                        effective_recipe.mtl_def_id.c_str());
            return false;
        }

        const bool program_changed = (material_comp->program != resolved_program);
        if (program_changed)
            InvalidateRecipeRuntime(material_comp, false);

        if (auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            for (const auto &req : resolved_program->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                    continue;

                const uint32_t stride = graph::mtl::GetSSBOTypeStructStride(req.ssbo_type);
                if (stride == 0)
                    continue;

                rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, stride);
            }
        }

        material_comp->program = resolved_program;
        material_comp->program_dirty = false;
        material_comp->recipe_hash = recipe_hash;
        material_comp->program_build_context_hash =
            build_context_hash;
        return true;
    }

    bool RenderPrimitiveCollectSystem::ResolveRuntimePipelineForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                                          const std::shared_ptr<MaterialComponent> &material_comp)
    {
        if (!world || !primitive_comp || !material_comp || !material_comp->program)
            return false;

        if (primitive_comp->GetOverridePipeline())
            return true;

        auto *render_context = world->GetRenderContext();
        auto *render_target = render_context ? render_context->GetCurrentRenderTarget() : world->GetRenderTarget();
        auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        if (!render_pass)
            return false;

        if (primitive_comp->GetResolvedRuntimePipeline()
         && primitive_comp->GetResolvedRuntimeRenderPass() != render_pass)
        {
            primitive_comp->ClearResolvedRuntimePipeline();
        }

        if (primitive_comp->GetResolvedRuntimePipeline())
            return true;

        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildEffectiveMaterialRecipe(primitive_comp, material_comp->program, effective_recipe))
            return false;

        const graph::VIL *vil = primitive_comp->GetRuntimeVIL();
        if (!vil)
            vil = material_comp->program->GetDefaultVIL();

        if (!vil)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveRuntimePipeline failed: missing effective VIL for %s material=%s",
                        GetPrimitiveOwnerName(primitive_comp),
                        material_comp->program ? material_comp->program->GetName().c_str() : "<null>");
            return false;
        }

        graph::Pipeline *resolved_pipeline = render_pass->CreatePipeline(material_comp->program,
                                                                         vil,
                                                                         effective_recipe);
        if (!resolved_pipeline)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveRuntimePipeline failed: CreatePipeline failed for %s material=%s",
                        GetPrimitiveOwnerName(primitive_comp),
                        material_comp->program ? material_comp->program->GetName().c_str() : "<null>");
            return false;
        }

        primitive_comp->SetResolvedRuntimePipeline(resolved_pipeline, render_pass);
        return true;
    }

    bool RenderPrimitiveCollectSystem::MaterializeRecipeRowsForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                                         const std::shared_ptr<MaterialComponent> &material_comp)
    {
        if (!world || !primitive_comp || !material_comp)
            return false;

        if (!MaterialRequiresRecipeRuntimeRows(material_comp->program))
        {
            material_comp->texture_layer_row = 0;
            material_comp->data_index_row = 0;
            material_comp->data_index_values.clear();
            material_comp->bindings_dirty = false;
            material_comp->resources_dirty = false;
            material_comp->valid = true;
            return true;
        }

        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildEffectiveMaterialRecipe(primitive_comp, material_comp->program, effective_recipe))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: BuildEffectiveMaterialRecipe failed for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        material_comp->ClearResolvedSSBOBindings();
        material_comp->data_index_values.clear();
        for (const auto &req : material_comp->program->GetMaterialResourceLayout().requirements)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                continue;

            uint32_t resolved_ssbo_id = 0;
            if (!ResolveRecipeSSBOBindingId(effective_recipe, req, resolved_ssbo_id))
            {
                GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: unresolved SSBO binding for %s descriptor=%s slot=%u type=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            req.name ? req.name : "<unnamed>",
                            req.data_slot,
                            graph::mtl::GetSSBOTypeName(req.ssbo_type));
                return false;
            }

            material_comp->SetResolvedSSBOBinding(
                req.name, req.data_slot, req.ssbo_type, resolved_ssbo_id);
        }

        auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: RenderDescriptorBindingSystem missing for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        graph::mtl::MaterializationSpec spec{};
        graph::mtl::MaterializationInstanceData instance_data{};
        uint32_t texture_layer_row = uint32_t(-1);
        uint32_t data_index_row = uint32_t(-1);
        if (!rdbs->ResolveMaterialRecipe(
                effective_recipe, spec, &texture_layer_row, &data_index_row, &instance_data))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialRecipe failed for %s recipe=%s",
                        GetPrimitiveOwnerName(primitive_comp),
                        effective_recipe.recipe_name.c_str());
            return false;
        }

        if (texture_layer_row == uint32_t(-1) || data_index_row == uint32_t(-1))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialRecipe returned invalid rows. tex=%u data=%u",
                        texture_layer_row,
                        data_index_row);
            return false;
        }

        // Determine the entity's own data_index from the effective recipe.
        // ResolveMaterialRecipe rehydrates this instance value from the shared
        // resource-resolution cache, so it never inherits another primitive's row.
        uint32_t entity_data_index = uint32_t(-1);
        for (const auto &asset_binding : effective_recipe.ssbo_assets)
        {
            if (asset_binding.use_data_index)
            {
                entity_data_index = asset_binding.data_index;
            }
        }

        material_comp->data_index_values = std::move(instance_data.data_index_values);

        for (const auto &asset_binding : effective_recipe.ssbo_assets)
        {
            if (!asset_binding.use_data_index)
                continue;

            uint32_t data_slot = asset_binding.data_slot;
            for (const auto &req : material_comp->program->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic == graph::mtl::DescriptorSemantic::MaterialDataSlotData
                 && req.name
                 && req.data_slot == asset_binding.data_slot
                 && req.ssbo_type == asset_binding.ssbo_type
                 && asset_binding.data_slot_name == req.name)
                {
                    data_slot = req.data_slot;
                    break;
                }
            }

            if (data_slot >= material_comp->data_index_values.size())
                material_comp->data_index_values.resize(data_slot + 1, 0u);
            material_comp->data_index_values[data_slot] = asset_binding.data_index;
        }

        if (entity_data_index != uint32_t(-1))
        {
            // Only texture-using materials need the legacy "texture row == data row" mirror.
            // Untextured materials (for example gizmo/pure-color) may legally reuse the same
            // data_index values as textured materials; writing an all-zero texture row
            // here would clobber the textured material's global handle table entry.
            if (!effective_recipe.textures.empty())
            {
                rdbs->WriteTextureLayerRowAt(entity_data_index, spec);
                texture_layer_row = entity_data_index;
            }

            data_index_row = entity_data_index;
        }
        else
        {
            data_index_row = ResolvePrimaryStructIndex(material_comp->program,
                                                       spec,
                                                       data_index_row);
        }

        instance_data.texture_layer_row = texture_layer_row;
        instance_data.data_index_row = data_index_row;
        material_comp->texture_layer_row = instance_data.texture_layer_row;
        material_comp->data_index_row = instance_data.data_index_row;
        material_comp->bindings_dirty = false;
        material_comp->resources_dirty = false;
        material_comp->valid = true;
        return true;
    }

    void RenderPrimitiveCollectSystem::Update(float /*deltaTime*/)
    {
        if (!world)
            return;

        // Lazily resolve cameraInfo from CameraSystem if not explicitly set
        // (CameraSystem may be registered after RegisterDefaultEcsSystems runs)
        if (!cameraInfo)
        {
            if (auto cam_sys = world->GetSystem<CameraSystem>())
                cameraInfo = cam_sys->GetCameraInfo();
        }

        if (!cameraInfo)
            return;

        auto& cache = world->GetRenderFrameCache();
        cache.cameraInfo = cameraInfo;
        cache.BeginFrame();

        // Get visibility storage for fast O(1) lookup
        VisibilityDataStorage* visibility_storage = nullptr;
        auto vis_system = world->GetSystem<VisibilitySystem>();
        if (vis_system)
        {
            visibility_storage = vis_system->GetStorage();
        }

        std::vector<std::shared_ptr<PrimitiveComponent>> primitives;
        world->GetComponents<PrimitiveComponent>(primitives);

        size_t skipped_invisible = 0;
        size_t skipped_no_owner = 0;
        size_t skipped_no_transform = 0;
        size_t added = 0;

        const glm::vec3 camera_pos = glm::vec3(cameraInfo->pos);

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp)
                continue;

            if (!primitiveComp->IsVisible() || !primitiveComp->CanRender())
            {
                if (!primitiveComp->IsVisible())
                {
                    ++skipped_invisible;
                }
                continue;
            }

            EntityID entity_id = primitiveComp->GetOwnerID();

            // Fast O(1) lookup from VisibilityDataStorage
            if (visibility_storage && visibility_storage->IsInvisible(entity_id))
            {
                ++skipped_invisible;
                continue;
            }

            Entity* entity = primitiveComp->GetOwner();
            if (!entity)
            {
                ++skipped_no_owner;
                continue;
            }

            if (!world->IsEntityRenderEnabled(entity))
                continue;

            if (!primitiveComp->HasAnyMaterialRecipeSource())
            {
                GLogWarning("[RenderPrimitiveCollectSystem] Skip primitive without recipe: %s",
                            primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>");
            }
            else
            {
                auto material_comp = entity->GetComponent<MaterialComponent>();
                if (!material_comp)
                    material_comp = entity->AddComponent<MaterialComponent>();

                if (!PrepareRecipeAuthoringResources(world, primitiveComp, nullptr))
                {
                    GLogWarning("[RenderPrimitiveCollectSystem] PrepareRecipeAuthoringResources(pre-resolve) failed for %s",
                                primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>");
                    InvalidateRecipeRuntime(material_comp, true);
                }
                else
                {
                    const bool resolved_program = ResolveMaterialProgramForPrimitive(primitiveComp, material_comp);
                    if (!resolved_program)
                    {
                        GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialProgramForPrimitive failed for %s",
                                    primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>");
                        InvalidateRecipeRuntime(material_comp, true);
                    }
                    else
                    {
                        if (!PrepareRecipeAuthoringResources(world, primitiveComp, material_comp->program))
                        {
                            GLogWarning("[RenderPrimitiveCollectSystem] PrepareRecipeAuthoringResources(post-resolve) failed for %s program=%s",
                                        primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>",
                                        material_comp->program ? material_comp->program->GetName().c_str() : "<null>");
                            InvalidateRecipeRuntime(material_comp, false);
                        }
                        else
                        {
                            const bool materialized_rows = MaterializeRecipeRowsForPrimitive(primitiveComp, material_comp);
                            if (!materialized_rows)
                            {
                                GLogWarning("[RenderPrimitiveCollectSystem] MaterializeRecipeRowsForPrimitive failed for %s program=%s",
                                            primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>",
                                            material_comp->program ? material_comp->program->GetName().c_str() : "<null>");
                                InvalidateRecipeRuntime(material_comp, false);
                            }
                            else if (!EnsureRuntimeGeometryFromAsset(world, primitiveComp, material_comp))
                            {
                                GLogWarning("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed for %s",
                                            primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>");
                            }
                            else if (!ResolveRuntimePipelineForPrimitive(primitiveComp, material_comp))
                            {
                                GLogWarning("[RenderPrimitiveCollectSystem] ResolveRuntimePipelineForPrimitive failed for %s",
                                            primitiveComp->GetOwner() ? primitiveComp->GetOwner()->GetName().c_str() : "<no-owner>");
                            }
                        }
                    }
                }
            }

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
            {
                ++skipped_no_transform;
                continue;
            }

            auto material_for_item = entity->GetComponent<MaterialComponent>();
            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, material_for_item, world);

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            item->distanceToCamera = glm::length(toCamera);

            item->UpdateWorldMatrix();

            cache.renderItems.push_back(std::unique_ptr<RenderItem>(std::move(item)));
            cache.renderableCount++;
            ++added;
        }

        //if (cache.renderableCount == 0)
        //{
        //    LogInfo("[RenderPrimitiveCollectSystem] No renderables: total=%zu visible=%zu no_owner=%zu no_transform=%zu",
        //             primitives.size(),
        //             added,
        //             skipped_no_owner,
        //             skipped_no_transform);
        //}
    }
}//namespace hgl::ecs
