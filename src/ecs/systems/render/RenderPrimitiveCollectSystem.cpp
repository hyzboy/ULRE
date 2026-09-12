#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/module/MaterialSSBOBufferRegistry.h>

#include<hgl/graph/ssbo/MaterialSSBOLayout.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/BindingTableBuilder.h>
#include<hgl/util/hash/FNV1a.h>
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
                GLogError("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed: material program null owner=%s valid=%d program_dirty=%d runtime_dirty=%d",
                          GetPrimitiveOwnerName(primitive_comp),
                          material_comp->valid ? 1 : 0,
                          material_comp->program_dirty ? 1 : 0,
                          material_comp->runtime_dirty ? 1 : 0);
                return false;
            }
            return primitive_comp->EnsureRuntimeGeometryBinding(material);
        }

        inline graph::mtl::MaterialSSBOType ResolveMaterialSSBORequirementType(
            const graph::mtl::ShaderResourceSlot &req) noexcept
        {
            return req.material_ssbo_type;
        }

        bool ResolveRecipeSSBOBindingId(const graph::mtl::MaterialRecipe &recipe,
                                        const graph::mtl::ShaderResourceSlot &req,
                                        uint32_t &out_ssbo_id)
        {
            const auto material_ssbo_type = ResolveMaterialSSBORequirementType(req);

            if (const auto *asset = graph::mtl::FindRecipeSSBOAssetBinding(
                    recipe,
                    req.name.c_str(),
                    req.material_private_data_slot,
                    material_ssbo_type))
            {
                out_ssbo_id = asset->ssbo_id;
                return true;
            }

            return false;
        }

        bool BuildResolvedRecipe(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                          const graph::ShaderProgram *material_program,
                                          graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!primitive_comp)
                return false;

            return primitive_comp->BuildResolvedAuthoringMaterialRecipe(out_recipe, material_program);
        }

        void LogMaterialBindingFailure(
            const char *owner_name,
            const graph::ShaderProgram *program,
            const graph::mtl::MaterialRecipe &recipe,
            const graph::mtl::ResolvedBindingTable &view)
        {
            GLogWarning(
                "[MaterialBinding] owner=%s program=%s ready=%d valid=%d missing=%u program_key=%llu view_hash=%llu expected_binding_hash=%llu actual_binding_hash=%llu recipe=%s definition=%s textures=%zu data=%zu",
                owner_name ? owner_name : "<null>",
                program ? program->GetName().c_str() : "<null>",
                view.IsRuntimeReady() ? 1 : 0,
                view.IsValid() ? 1 : 0,
                view.missing_required_count,
                static_cast<unsigned long long>(
                    view.program_key_digest),
                static_cast<unsigned long long>(view.GetStableHash()),
                static_cast<unsigned long long>(
                    view.source_binding_hash),
                static_cast<unsigned long long>(
                    graph::mtl::GetBindingSourceHash(
                        recipe)),
                recipe.recipe_name.c_str(),
                recipe.mtl_def_id.c_str(),
                recipe.textures.size(),
                recipe.ssbo_assets.size());

            for (int i = 0; i < view.textures.GetCount(); ++i)
            {
                const auto &binding = view.textures[i];
                if (binding.source
                        != graph::mtl::BindingSource::Missing)
                    continue;
                GLogWarning(
                    "[MaterialBinding][MissingTexture] view_index=%d logical=%llu name=%s source=%s required=%d allow_fallback=%d recipe_index=%u asset_hash=%llu metadata_hash=%llu",
                    i,
                    static_cast<unsigned long long>(
                        binding.logical_resource_id),
                    binding.texture_name,
                    graph::mtl::GetBindingSourceName(
                        binding.source),
                    binding.required ? 1 : 0,
                    binding.allow_fallback ? 1 : 0,
                    binding.recipe_binding_index,
                    static_cast<unsigned long long>(
                        binding.asset_identity_hash),
                    static_cast<unsigned long long>(
                        binding.asset_metadata_hash));
            }
            for (int i = 0; i < view.data.GetCount(); ++i)
            {
                const auto &binding = view.data[i];
                if (binding.source
                        != graph::mtl::BindingSource::Missing)
                    continue;
                GLogWarning(
                    "[MaterialBinding][MissingData] view_index=%d logical=%llu slot=%u type=%s(%u) source=%s required=%d allow_fallback=%d recipe_index=%u ssbo_id=%u data_index=%u use_data_index=%d shared=%d asset_hash=%llu metadata_hash=%llu",
                    i,
                    static_cast<unsigned long long>(
                        binding.logical_resource_id),
                    binding.material_private_data_slot,
                    graph::mtl::GetMaterialSSBOTypeName(binding.ssbo_type),
                    static_cast<uint32_t>(binding.ssbo_type),
                    graph::mtl::GetBindingSourceName(
                        binding.source),
                    binding.required ? 1 : 0,
                    binding.allow_fallback ? 1 : 0,
                    binding.recipe_binding_index,
                    binding.ssbo_id,
                    binding.data_index,
                    binding.use_data_index ? 1 : 0,
                    binding.shared_across_instances ? 1 : 0,
                    static_cast<unsigned long long>(
                        binding.asset_identity_hash),
                    static_cast<unsigned long long>(
                        binding.asset_metadata_hash));
            }
            for (size_t i = 0; i < recipe.textures.size(); ++i)
            {
                const auto &binding = recipe.textures[i];
                GLogWarning(
                    "[MaterialBinding][RecipeTexture] index=%zu name=%s resource=%s layer=%u required=%d",
                    i,
                    binding.texture_name.c_str(),
                    binding.resource_id.c_str(),
                    binding.array_layer,
                    binding.required ? 1 : 0);
            }
            for (size_t i = 0; i < recipe.ssbo_assets.size(); ++i)
            {
                const auto &binding = recipe.ssbo_assets[i];
                GLogWarning(
                    "[MaterialBinding][RecipeData] index=%zu name=%s slot=%u type=%s(%u) ssbo_id=%u data_index=%u use_data_index=%d shared=%d",
                    i,
                    binding.material_private_data_slot_name.c_str(),
                    binding.material_private_data_slot,
                    graph::mtl::GetMaterialSSBOTypeName(binding.ssbo_type),
                    static_cast<uint32_t>(binding.ssbo_type),
                    binding.ssbo_id,
                    binding.data_index,
                    binding.use_data_index ? 1 : 0,
                    binding.shared_across_instances ? 1 : 0);
            }
            if (!program)
                return;
            const auto &requirements =
                program->GetShaderResourceSchema().resources;
            for (size_t i = 0; i < requirements.size(); ++i)
            {
                const auto &requirement = requirements[i];
                if (requirement.semantic
                        != graph::mtl::DescriptorSemantic::MaterialTexture
                 && requirement.semantic
                        != graph::mtl::DescriptorSemantic::MaterialSampler
                 && requirement.semantic
                        != graph::mtl::DescriptorSemantic::
                            MaterialPrivateData)
                    continue;
                GLogWarning(
                    "[MaterialBinding][Layout] index=%zu name=%s semantic=%s layer=%s required=%d allow_fallback=%d material_private_data_slot=%u type=%s(%u) ssbo_id=%u",
                    i,
                    requirement.name.empty() ? "<unnamed>" : requirement.name.c_str(),
                    graph::mtl::GetDescriptorSemanticName(
                        requirement.semantic),
                    graph::mtl::GetDescriptorSemanticLayerName(
                        requirement.semantic_layer),
                    requirement.required ? 1 : 0,
                    requirement.allow_fallback ? 1 : 0,
                    requirement.material_private_data_slot,
                    graph::mtl::GetSSBOTypeName(requirement.ssbo_type),
                    static_cast<uint32_t>(requirement.ssbo_type),
                    requirement.ssbo_id);
            }
        }

        bool PrepareActivePlanResources(
            ECSContext *world,
            const std::shared_ptr<PrimitiveComponent> &primitive_comp,
            graph::ShaderProgram *material_program,
            const graph::mtl::ResolvedBindingTable &binding_table)
        {
            if (!world
             || !primitive_comp
             || !material_program
             || !binding_table.IsRuntimeReady())
                return false;

            graph::mtl::MaterialRecipe active_recipe{};
            if (!BuildResolvedRecipe(
                    primitive_comp, material_program, active_recipe))
                return false;

            auto rdbs = world->GetSystem<RenderSceneUBOSystem>();
            auto *render_context = world->GetRenderContext();
            auto *graphics_context = render_context
                ? render_context->GetGraphicsContext()
                : world->GetGraphicsContext();
            auto *bindless_mgr = graphics_context
                ? graphics_context->
                    GetManager<graph::BindlessTextureManager>()
                : nullptr;
            if (!rdbs)
                return false;

            const char *owner_name =
                GetPrimitiveOwnerName(primitive_comp);

            for (int i = 0; i < binding_table.textures.GetCount(); ++i)
            {
                const graph::mtl::ResolvedTextureBinding &binding =
                    binding_table.textures[i];
                if (binding.source
                        != graph::mtl::BindingSource::Asset)
                    continue;

                const graph::mtl::RecipeTextureBinding
                    *recipe_binding = nullptr;
                if (binding.recipe_binding_index
                        < active_recipe.textures.size())
                {
                    const graph::mtl::RecipeTextureBinding
                        &candidate = active_recipe.textures[
                            binding.recipe_binding_index];
                    if (candidate.texture_name == binding.texture_name
                     && graph::mtl::
                            GetResolvedTextureAssetIdentityHash(
                                candidate.resource_id.data(),
                                static_cast<uint32_t>(
                                    candidate.resource_id.size()))
                            == binding.asset_identity_hash)
                    {
                        recipe_binding = &candidate;
                    }
                }
                const auto *resource =
                    primitive_comp->GetMaterialTextureResource(
                        binding.texture_name);
                if (!recipe_binding
                 || !resource
                 || !bindless_mgr)
                {
                    GLogError(
                        "[DeferredResource] Texture acquisition failed: owner=%s texture=%s recipe=%d resource=%d bindless=%d",
                        owner_name,
                        binding.texture_name[0] == '\0'
                            ? "<unnamed>" : binding.texture_name,
                        recipe_binding ? 1 : 0,
                        resource ? 1 : 0,
                        bindless_mgr ? 1 : 0);
                    return false;
                }

                const std::string resource_id =
                    resource->resource_id.empty()
                        ? BuildTextureResourceId(resource->texture)
                        : resource->resource_id;
                if (graph::mtl::
                        GetResolvedTextureAssetIdentityHash(
                            resource_id.data(),
                            static_cast<uint32_t>(
                                resource_id.size()))
                        != binding.asset_identity_hash)
                {
                    GLogError(
                        "[DeferredResource] Texture identity mismatch: owner=%s texture=%s",
                        owner_name,
                        binding.texture_name[0] == '\0'
                            ? "<unnamed>" : binding.texture_name);
                    return false;
                }

                // 所有纹理（2D / 2DArray）统一走 bindless Register。
                // resource->kind 仅保留用于资产加载（authoring）分支，
                // 描述符侧 2D 与 2DArray 均落在 sampler2DArray[]（单层/多层）。
                uint32_t handle = rdbs->RegisterTextureResource(
                    resource_id,
                    resource->texture,
                    bindless_mgr);
                if (handle == 0)
                    return false;
            }

            for (int i = 0; i < binding_table.data.GetCount(); ++i)
            {
                const graph::mtl::ResolvedDataBinding &binding =
                    binding_table.data[i];
                if (binding.source
                        != graph::mtl::BindingSource::Asset)
                    continue;

                const graph::mtl::RecipeSSBOAssetBinding
                    *recipe_binding = nullptr;
                if (binding.recipe_binding_index
                        < active_recipe.ssbo_assets.size())
                {
                    const graph::mtl::RecipeSSBOAssetBinding
                        &candidate = active_recipe.ssbo_assets[
                            binding.recipe_binding_index];
                    if (candidate.material_private_data_slot == binding.material_private_data_slot
                     && candidate.ssbo_type == binding.ssbo_type
                     && graph::mtl::GetResolvedDataAssetIdentityHash(
                            candidate.ssbo_type,
                            candidate.ssbo_id,
                            candidate.material_private_data_slot)
                            == binding.asset_identity_hash)
                    {
                        recipe_binding = &candidate;
                    }
                }
                if (!recipe_binding)
                    return false;
            }
            return true;
        }

        void InvalidateRecipeRuntime(const std::shared_ptr<MaterialComponent> &material_comp,
                                     const bool clear_program)
        {
            if (!material_comp)
                return;

            if (material_comp->material_texture_configuration.IsValid())
            {
                auto *graphics_context = material_comp->GetOwner()
                    && material_comp->GetOwner()->GetContext()
                    ? material_comp->GetOwner()->GetContext()
                        ->GetGraphicsContext()
                    : nullptr;
                auto *registry = graphics_context
                    ? graphics_context->GetSSBOBufferRegistry()
                    : nullptr;
                if (registry)
                {
                    const uint64_t retire_epoch =
                        static_cast<uint64_t>(
                            material_comp->GetOwner()->GetContext()
                                ->GetRenderSubmissionSerial())
                        + graph::MaterialTextureConfigurationRetireEpochDelay;
                    if (registry->IsMaterialTextureConfigurationValid(
                            material_comp->material_texture_configuration))
                    {
                        registry->RetireMaterialTextureConfiguration(
                            material_comp->material_texture_configuration,
                            retire_epoch);
                    }
                }
            }

            material_comp->ClearMaterializationRows();
            material_comp->runtime_dirty = true;
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
        SetExecutionPhase(ExecutionPhase::RenderCollect);
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

        // P3: Fast-path — if nothing has changed since last resolve, skip all work.
        if (!material_comp->program_dirty
            && material_comp->program
            && material_comp->tracked_material_authored_generation == primitive_comp->GetMaterialAuthoredGeneration()
            && material_comp->resolved_binding_table.IsRuntimeReady())
            return true;

        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildResolvedRecipe(primitive_comp, nullptr, effective_recipe))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] BuildResolvedRecipe failed for %s",
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

        // 渲染变体 purpose 必须先于脏检查解析——若 Forward↔Shadow 切换而
        // recipe/geometry/profile 不变，哈希不含 purpose 会复用错误的 program
        graph::mtl::ShaderProgramPurpose effective_purpose =
            graph::mtl::ShaderProgramPurpose::ForwardColor;
        switch (primitive_comp->GetPrimitiveVariantPurpose())
        {
        case graph::PrimitiveVariantPurpose::DepthOnly:
            effective_purpose =
                graph::mtl::ShaderProgramPurpose::DepthOnly;
            break;
        case graph::PrimitiveVariantPurpose::ShadowCaster:
            effective_purpose =
                graph::mtl::ShaderProgramPurpose::ShadowDepth;
            break;
        default:
            break;
        }

        const uint64_t build_context_hash =
            graph::mtl::HashMaterialProgramBuildContext(
                primitive_type,
                geometry_vertex_format,
                graphics->GetPhysicalDeviceProfile(),
                effective_purpose);
        if (material_comp->recipe_hash != recipe_hash
         || material_comp->program_build_context_hash
                != build_context_hash)
        {
            material_comp->program_dirty = true;
            InvalidateRecipeRuntime(material_comp, false);
        }

        if (!material_comp->program_dirty
         && material_comp->program
         && material_comp->resolved_binding_table.IsRuntimeReady())
        {
            // Generation may have advanced without changing the recipe/program
            // content (e.g. an author swapped a texture or data object but kept
            // the same resource id). Refresh the tracked generation and flag
            // runtime dirty so PrepareActivePlanResources re-registers the
            // current resource objects on the next Update.
            if (material_comp->tracked_material_authored_generation
                != primitive_comp->GetMaterialAuthoredGeneration())
            {
                material_comp->runtime_dirty = true;
                material_comp->tracked_material_authored_generation =
                    primitive_comp->GetMaterialAuthoredGeneration();
            }
            return true;
        }

        // SceneGraph owns the existing provider-shape routing policy. It
        // selects a concrete template request before ShaderGen is invoked.
        graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        mtl_request.shader_program_purpose = effective_purpose;
        graph::mtl::MaterialDefinition template_definition{};
        if (!graph::mtl::TryGetMaterialDefinitionByID(
                effective_recipe.mtl_def_id, template_definition)
         && !graph::mtl::TryGetMaterialDefinitionByID(
                graph::mtl::GetFallbackMaterialDefinitionID(),
                template_definition))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Cannot select template for material=%s",
                effective_recipe.mtl_def_id.c_str());
            return false;
        }
        if (!graph::SelectCurrentSceneRenderTemplateRequest(
                template_definition, mtl_request,
                mtl_request.render_template_request))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Template selection failed for material=%s",
                effective_recipe.mtl_def_id.c_str());
            return false;
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

        graph::mtl::MaterialRecipe material_binding_recipe{};
        graph::mtl::ResolvedBindingTable binding_table{};
        graph::mtl::BindingBuildDiagnostic
            binding_diagnostic{};
        if (!BuildResolvedRecipe(
                primitive_comp,
                resolved_program,
                material_binding_recipe)
         || !graph::mtl::BuildBindingTable(
                material_binding_recipe,
                resolved_program->GetShaderResourceSchema(),
                resolved_program->GetProgramKey(),
                &template_definition,
                binding_table,
                binding_diagnostic))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Material Binding View build failed for %s error=%s",
                GetPrimitiveOwnerName(primitive_comp),
                graph::mtl::GetBindingBuildErrorName(
                    binding_diagnostic.error));
            return false;
        }
        if (!binding_table.IsRuntimeReady())
        {
            LogMaterialBindingFailure(
                GetPrimitiveOwnerName(primitive_comp),
                resolved_program,
                material_binding_recipe,
                binding_table);
        }

        const bool program_changed = (material_comp->program != resolved_program);
        if (program_changed)
            InvalidateRecipeRuntime(material_comp, false);

        if (auto rdbs = world->GetSystem<RenderSceneUBOSystem>())
        {
            for (const auto &req : resolved_program->GetShaderResourceSchema().resources)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                    continue;

                const graph::mtl::MaterialSSBOType material_ssbo_type =
                    ResolveMaterialSSBORequirementType(req);
                const uint32_t stride = graph::mtl::GetMaterialSSBOTypeStructStride(material_ssbo_type);
                if (stride == 0)
                    continue;

                rdbs->RegisterMaterialStructLayout(material_ssbo_type, req.ssbo_id, stride);
            }
        }

        material_comp->program = resolved_program;
        material_comp->resolved_binding_table = binding_table;
        if (binding_table.IsRuntimeReady())
        {
            uint32_t planned_textures = 0;
            uint32_t planned_data = 0;
            for (int i = 0; i < binding_table.textures.GetCount(); ++i)
            {
                if (binding_table.textures[i].source
                        == graph::mtl::BindingSource::Asset)
                    ++planned_textures;
            }
            for (int i = 0; i < binding_table.data.GetCount(); ++i)
            {
                if (binding_table.data[i].source
                        == graph::mtl::BindingSource::Asset)
                    ++planned_data;
            }
            GLogVerbose(
                "[DeferredResource] owner=%s program=%s table_hash=%llu planned_texture=%u planned_data=%u recipe_texture=%zu recipe_data=%zu unused_texture=%u unused_data=%u",
                GetPrimitiveOwnerName(primitive_comp),
                resolved_program->GetName().c_str(),
                static_cast<unsigned long long>(
                    binding_table.GetStableHash()),
                planned_textures,
                planned_data,
                material_binding_recipe.textures.size(),
                material_binding_recipe.ssbo_assets.size(),
                binding_table.unused_recipe_texture_count,
                binding_table.unused_recipe_data_count);
        }
        material_comp->program_dirty = false;
        material_comp->MarkProgramResolved();
        material_comp->recipe_hash = recipe_hash;
        material_comp->program_build_context_hash =
            build_context_hash;

        // effective_recipe 出自 BuildResolvedAuthoringMaterialRecipe（组件边界
        // 已 NormalizeRecipe），直接缓存供 CreatePipeline 使用，无需再规范化。
        material_comp->cached_normalized_recipe = effective_recipe;

        // P3: Cache effective recipe (with program-resolved SSBO types)
        // to avoid redundant BuildResolvedRecipe downstream.
        material_comp->cached_effective_recipe = material_binding_recipe;
        material_comp->cached_effective_recipe_hash =
            graph::mtl::HashMaterialRecipe(material_binding_recipe);

        // P2-1: Project the pruned binding recipe back from the freshly built
        // binding table exactly once. Both inputs are in scope here, so the
        // recipe→table→recipe round-trip does not need to re-run on every
        // materialize. When the table is not runtime-ready the projection
        // fails and cached_binding_recipe_valid stays false, which makes
        // MaterializeRecipeRowsForPrimitive fail exactly as before.
        material_comp->cached_binding_recipe_valid =
            graph::mtl::BuildBindingTableRecipe(
                material_binding_recipe,
                binding_table,
                material_comp->cached_binding_recipe);
        material_comp->tracked_material_authored_generation = primitive_comp->GetMaterialAuthoredGeneration();

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

        graph::mtl::MaterialRecipe effective_recipe = material_comp->cached_effective_recipe;

        // Reuse cached normalized recipe when effective recipe hasn't changed
        // since it was last normalized in ResolveMaterialProgramForPrimitive.
        if (material_comp->recipe_hash
         == material_comp->cached_effective_recipe_hash)
            effective_recipe = material_comp->cached_normalized_recipe;

        graph::Pipeline *resolved_pipeline = render_pass->CreatePipeline(material_comp->program,
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

        if (!material_comp->program
         || !graph::mtl::MaterialRequiresRecipeRuntimeRows(
                material_comp->program->GetShaderResourceSchema()))
        {
            material_comp->data_index_row = 0;
            material_comp->runtime_dirty = false;
            material_comp->valid = false;
            return true;
        }

        // P3: use cached effective recipe instead of rebuilding
        graph::mtl::MaterialRecipe &effective_recipe = material_comp->cached_effective_recipe;
        graph::mtl::MaterialRecipe &material_binding_recipe = material_comp->cached_binding_recipe;
        if (!material_comp->cached_binding_recipe_valid)
        {
            LogMaterialBindingFailure(
                GetPrimitiveOwnerName(primitive_comp),
                material_comp->program,
                effective_recipe,
                material_comp->resolved_binding_table);
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Materialize failed: Material Binding View invalid for %s ready=%d missing=%u expected_binding_hash=%llu actual_binding_hash=%llu",
                GetPrimitiveOwnerName(primitive_comp),
                material_comp->resolved_binding_table.
                    IsRuntimeReady() ? 1 : 0,
                material_comp->resolved_binding_table.
                    missing_required_count,
                static_cast<unsigned long long>(
                    material_comp->resolved_binding_table.
                        source_binding_hash),
                static_cast<unsigned long long>(
                    graph::mtl::GetBindingSourceHash(
                        effective_recipe)));
            return false;
        }

        if (getenv("ULRE_ARENA_DEBUG"))
            GLogInfo("[ArenaTrace] materialize entry: ssbo_assets=%u binding_valid=%d schema_reqs=%u",
                     (uint32_t)material_binding_recipe.ssbo_assets.size(),
                     material_comp->cached_binding_recipe_valid ? 1 : 0,
                     (uint32_t)material_comp->program->GetShaderResourceSchema().resources.size());

        material_comp->ClearResolvedSSBOBindings();
        for (const auto &req : material_comp->program->GetShaderResourceSchema().resources)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                continue;

            uint32_t resolved_ssbo_id = 0;
            if (!ResolveRecipeSSBOBindingId(
                    material_binding_recipe, req, resolved_ssbo_id))
            {
                GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: unresolved SSBO binding for %s descriptor=%s slot=%u type=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            req.name.empty() ? "<unnamed>" : req.name.c_str(),
                            req.material_private_data_slot,
                            graph::mtl::GetMaterialSSBOTypeName(
                                ResolveMaterialSSBORequirementType(req)));
                return false;
            }

            material_comp->SetResolvedSSBOBinding(
                req.name.c_str(),
                req.material_private_data_slot,
                ResolveMaterialSSBORequirementType(req),
                resolved_ssbo_id);
        }

        auto rdbs = world->GetSystem<RenderSceneUBOSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: RenderSceneUBOSystem missing for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        // Determine the entity's own active material row ID from the cached
        // binding recipe. The old shared MaterializationSpec cache is gone:
        // this value is always the primitive's explicitly authored DataID.
        //
        // scope_ssbo_id is the data-slot asset's SSBO id — the same scope the
        // per-batch data rows and the engine-managed texture-layer rows domain
        // SSBO are keyed by. Materials without any data slot (TextureQuad /
        // TextDrawTest) fall back to a program-derived scope id and own row 0.
        uint32_t entity_data_index = uint32_t(-1);
        uint32_t fallback_data_index = uint32_t(-1);
        uint32_t scope_ssbo_id = 0;
        for (const auto &asset_binding : material_binding_recipe.ssbo_assets)
        {
            if (asset_binding.use_data_index)
            {
                entity_data_index = asset_binding.data_index;
                scope_ssbo_id = asset_binding.ssbo_id;
            }
            else if (fallback_data_index == uint32_t(-1))
            {
                fallback_data_index = asset_binding.data_index;
                scope_ssbo_id = asset_binding.ssbo_id;
            }
        }
        if (entity_data_index == uint32_t(-1))
            entity_data_index = fallback_data_index;

        if (scope_ssbo_id == 0)
        {
            scope_ssbo_id = graph::mtl::MakeECSSSBOId(
                static_cast<uint32_t>(material_comp->program->GetProgramKey().GetDigest())
                & graph::mtl::SSBOIdLocalMask);
        }

        // Fill the per-batch material data index table for every SSBO asset,
        // including use_data_index == false ones (the shader still reads
        // data[data_index], so the authored index must be published in the table).
        // 单槽化：材质唯一私有数据 SSBO 固定 slot 0。
        for (const auto &asset_binding : material_binding_recipe.ssbo_assets)
        {

            // data_index is the active row ID in this type's shared material
            // buffer; translate it to the CPU/GPU address used by the current ABI.
            {
                static bool arena_trace_done = false;
                if (getenv("ULRE_ARENA_DEBUG") && !arena_trace_done)
                {
                    arena_trace_done = true;
                    GLogInfo("[ArenaTrace] materialize: ssbo_id=%u data_index=%u assets=%u",
                             asset_binding.ssbo_id,
                             asset_binding.data_index,
                             (uint32_t)material_binding_recipe.ssbo_assets.size());
                }

                material_comp->material_row_cpu = nullptr;
                material_comp->material_row_gpu     = 0;

                if (!graph::mtl::IsMaterialSSBOType(
                        asset_binding.ssbo_type)
                 || asset_binding.ssbo_id == 0
                 || !asset_binding.use_data_index
                 || asset_binding.data_index == uint32_t(-1))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: invalid material row binding for %s type=%s ssbo_id=%u data_index=%u use_data_index=%d",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::mtl::GetMaterialSSBOTypeName(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index,
                        asset_binding.use_data_index ? 1 : 0);
                    return false;
                }

                auto *graphics_context = world->GetGraphicsContext();
                auto *material_domain = graphics_context
                    ? graphics_context->GetMaterialSSBOBufferRegistry()
                    : nullptr;
                graph::MaterialRowBufferInfo material_buffer{};
                if (!material_domain
                 || !material_domain->TryGetRowBuffer(
                        asset_binding.ssbo_id,
                        material_buffer))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: material row buffer missing for %s type=%s ssbo_id=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::mtl::GetMaterialSSBOTypeName(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id);
                    return false;
                }
                if (!material_domain->IsMaterialDataIDActive(
                        asset_binding.ssbo_type,
                        asset_binding.data_index))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: inactive material row ID for %s type=%s ssbo_id=%u data_index=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::mtl::GetMaterialSSBOTypeName(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index);
                    return false;
                }
                if (material_buffer.material_ssbo_type
                        != asset_binding.ssbo_type
                 || material_buffer.gpu_base == 0
                 || material_buffer.row_bytes == 0
                 || material_buffer.row_capacity == 0
                 || asset_binding.data_index >= material_buffer.row_capacity)
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: material row buffer invalid for %s type=%s buffer_type=%s ssbo_id=%u data_index=%u capacity=%u row_bytes=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::mtl::GetMaterialSSBOTypeName(
                            asset_binding.ssbo_type),
                        graph::mtl::GetMaterialSSBOTypeName(
                            material_buffer.material_ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index,
                        material_buffer.row_capacity,
                        material_buffer.row_bytes);
                    return false;
                }

                const uint64_t row_offset =
                    uint64_t(asset_binding.data_index) * material_buffer.row_bytes;
                material_comp->material_row_cpu = material_buffer.cpu_base
                    ? static_cast<uint8_t *>(material_buffer.cpu_base) + row_offset
                    : nullptr;
                material_comp->material_row_gpu = material_buffer.gpu_base + row_offset;

                if (!arena_trace_done)
                {
                    GLogInfo(
                        "[ArenaTrace] translated: gpu=0x%llx (base=%llu row_bytes=%u domain=material)",
                        (unsigned long long)material_comp->material_row_gpu,
                        (unsigned long long)material_buffer.gpu_base,
                        material_buffer.row_bytes);
                }
                continue;
            }

        }

        // data_index（行号）仍按 data_index VALUE 发布——行表/行尾镜像共用。
        material_comp->data_index_row =
            entity_data_index != uint32_t(-1) ? entity_data_index : 0u;

        auto *texture_graphics_context = world->GetGraphicsContext();
        auto *texture_registry = texture_graphics_context
            ? texture_graphics_context->GetSSBOBufferRegistry()
            : nullptr;

        graph::mtl::MaterialDefinition texture_definition{};
        graph::mtl::MaterialTextureReferenceLayout texture_layout{};
        bool has_texture_definition =
            !effective_recipe.mtl_def_id.empty()
         && graph::mtl::TryGetMaterialDefinitionByID(
                effective_recipe.mtl_def_id,
                texture_definition);
        if (!has_texture_definition)
        {
            has_texture_definition =
                graph::mtl::TryGetMaterialDefinitionByID(
                    graph::mtl::GetFallbackMaterialDefinitionID(),
                    texture_definition);
        }

        if (!has_texture_definition
         || !graph::mtl::BuildMaterialTextureReferenceLayout(
                texture_definition,
                texture_layout))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Materialize failed: texture definition lookup/layout failed for %s definition=%s",
                GetPrimitiveOwnerName(primitive_comp),
                effective_recipe.mtl_def_id.empty()
                    ? "<empty>" : effective_recipe.mtl_def_id.c_str());
            return false;
        }

        if (texture_registry)
            texture_registry->CollectRetiredMaterialTextureConfigurations(
                world->GetRenderSubmissionSerial());

        const uint64_t retire_epoch =
            static_cast<uint64_t>(world->GetRenderSubmissionSerial())
            + graph::MaterialTextureConfigurationRetireEpochDelay;

        if (texture_layout.HasReferences())
        {
            ValueArray<graph::mtl::MaterialTextureReference> references;
            references.Resize(static_cast<int>(texture_layout.reference_count));
            for (int i = 0; i < references.GetCount(); ++i)
                references[i] = {};

            for (size_t declaration_index = 0;
                 declaration_index < texture_definition.texture_declarations.size();
                 ++declaration_index)
            {
                const auto &declaration =
                    texture_definition.texture_declarations[declaration_index];
                const graph::mtl::RecipeTextureBinding *recipe_binding = nullptr;
                for (const auto &candidate : material_binding_recipe.textures)
                {
                    if (candidate.texture_name == declaration.name)
                    {
                        recipe_binding = &candidate;
                        break;
                    }
                }

                if (!recipe_binding)
                {
                    if (declaration.required)
                    {
                        GLogError(
                            "[RenderPrimitiveCollectSystem] Required texture binding missing: owner=%s texture=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            declaration.name.c_str());
                        return false;
                    }
                    continue;
                }

                if (recipe_binding->array_layer != 0
                 && !graph::mtl::IsMaterialTextureArraySampler(
                        declaration.sampler_type))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Non-array texture received array layer: owner=%s texture=%s layer=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        declaration.name.c_str(),
                        recipe_binding->array_layer);
                    return false;
                }

                uint32_t handle = 0;
                if (!recipe_binding->resource_id.empty())
                {
                    handle = rdbs->GetBindlessHandle(
                        AnsiString(recipe_binding->resource_id.c_str()));

                    if (handle == 0)
                    {
                        const auto *authoring =
                            primitive_comp->GetMaterialTextureResource(
                                declaration.name);
                        if (authoring
                         && authoring->texture)
                        {
                            const std::string fallback_id =
                                authoring->resource_id.empty()
                                    ? BuildTextureResourceId(authoring->texture)
                                    : authoring->resource_id;
                            handle = rdbs->GetBindlessHandle(
                                AnsiString(fallback_id.c_str()));
                        }
                    }
                }

                if (handle == 0 && declaration.required)
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Required texture handle missing: owner=%s texture=%s resource=%s",
                        GetPrimitiveOwnerName(primitive_comp),
                        declaration.name.c_str(),
                        recipe_binding->resource_id.empty()
                            ? "<direct/empty>"
                            : recipe_binding->resource_id.c_str());
                    return false;
                }

                references[static_cast<int>(declaration_index)] = {
                    handle,
                    handle == 0 ? 0u : recipe_binding->array_layer};
            }

            hgl::hash::FNV1aHasher64 reference_hasher;
            reference_hasher << texture_layout.layout_hash
                             << texture_layout.reference_count;
            for (int i = 0; i < references.GetCount(); ++i)
            {
                reference_hasher << references[i].descriptor_index
                                 << references[i].array_layer;
            }
            const uint64_t reference_configuration_hash =
                reference_hasher;

            if (!texture_registry)
            {
                GLogError(
                    "[RenderPrimitiveCollectSystem] Materialize failed: SSBO registry missing for texture references owner=%s",
                    GetPrimitiveOwnerName(primitive_comp));
                return false;
            }

            const uint64_t pool_key =
                graph::MaterialTextureReferencePool::MakePoolKey(
                    texture_definition,
                    texture_layout);
            const auto old_allocation =
                material_comp->material_texture_configuration;
            const bool old_allocation_live =
                old_allocation.IsValid()
             && texture_registry->IsMaterialTextureConfigurationValid(
                    old_allocation);
            const bool can_reuse =
                old_allocation_live
             && old_allocation.pool_key == pool_key
             && old_allocation.reference_count
                    == texture_layout.reference_count
             && old_allocation.row_stride == texture_layout.row_stride
             && material_comp->material_texture_configuration_hash
                    == reference_configuration_hash;

            graph::MaterialTextureConfigurationAllocation new_allocation =
                old_allocation;
            if (!can_reuse)
            {
                if (!texture_registry->AcquireMaterialTextureConfiguration(
                        texture_definition,
                        texture_layout,
                        new_allocation))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: texture configuration capacity exhausted owner=%s definition=%s",
                        GetPrimitiveOwnerName(primitive_comp),
                        effective_recipe.mtl_def_id.c_str());
                    return false;
                }
            }

            if (!can_reuse
             && !texture_registry->WriteMaterialTextureConfiguration(
                    new_allocation,
                    references.GetData(),
                    texture_layout.reference_count))
            {
                if (!can_reuse)
                    texture_registry->RetireMaterialTextureConfiguration(
                        new_allocation,
                        retire_epoch);
                GLogError(
                    "[RenderPrimitiveCollectSystem] Materialize failed: texture configuration write failed owner=%s",
                    GetPrimitiveOwnerName(primitive_comp));
                return false;
            }

            if (!can_reuse && old_allocation_live)
                texture_registry->RetireMaterialTextureConfiguration(
                    old_allocation,
                    retire_epoch);

            material_comp->material_texture_configuration = new_allocation;
            material_comp->material_texture_row_cpu =
                new_allocation.cpu_row;
            material_comp->material_texture_row_gpu =
                new_allocation.gpu_row;
            material_comp->material_texture_zero_row_gpu =
                texture_registry->
                    GetMaterialTextureConfigurationZeroRowAddress(
                        texture_definition,
                        texture_layout);
            if (material_comp->material_texture_zero_row_gpu == 0)
            {
                GLogError(
                    "[RenderPrimitiveCollectSystem] Materialize failed: texture configuration zero row unavailable owner=%s",
                    GetPrimitiveOwnerName(primitive_comp));
                return false;
            }
            material_comp->material_texture_configuration_hash =
                reference_configuration_hash;

            if (getenv("ULRE_ARENA_DEBUG"))
            {
                GLogInfo(
                    "[MaterialTextureReferences] owner=%s definition=%s row=%u references=%u gpu=0x%llx",
                    GetPrimitiveOwnerName(primitive_comp),
                    texture_definition.definition_id.c_str(),
                    new_allocation.row_index,
                    texture_layout.reference_count,
                    static_cast<unsigned long long>(
                        new_allocation.gpu_row));
                for (size_t i = 0;
                     i < texture_definition.texture_declarations.size();
                     ++i)
                {
                    const auto &declaration =
                        texture_definition.texture_declarations[i];
                    const auto &reference =
                        references[static_cast<int>(i)];
                    GLogInfo(
                        "[MaterialTextureReferences] texture=%s descriptor=%u layer=%u",
                        declaration.name.c_str(),
                        reference.descriptor_index,
                        reference.array_layer);
                }
            }
        }
        else
        {
            if (texture_registry
             && texture_registry->IsMaterialTextureConfigurationValid(
                    material_comp->material_texture_configuration))
                texture_registry->RetireMaterialTextureConfiguration(
                    material_comp->material_texture_configuration,
                    retire_epoch);
            material_comp->material_texture_configuration = {};
            material_comp->material_texture_row_cpu = nullptr;
            material_comp->material_texture_row_gpu = 0;
            material_comp->material_texture_zero_row_gpu = 0;
        }

        material_comp->runtime_dirty = false;
        material_comp->valid = false;
        material_comp->last_materialize_epoch = materialize_epoch;
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

        // P1-1: Global frame-level materialize gating.
        //
        // PrepareActivePlanResources and MaterializeRecipeRowsForPrimitive are
        // the per-frame hotspot: they re-traverse the resource plan, rebuild the
        // binding recipe and re-materialize rows on every Update even when
        // nothing changed. This pre-scan mirrors the resolve fast-path
        // (ResolveMaterialProgramForPrimitive), the materialization epoch and
        // the component success state to decide whether ANY primitive needs
        // work this frame. When none does, the main loop reuses last frame's
        // results and skips those two calls entirely.
        //
        // Epoch semantics: a materialize pass wipes per-primitive runtime rows
        // (rows are rebuilt from the resolved_ssbo_bindings / texture layer
        // values each time a primitive is materialized). A primitive skipped
        // this frame (e.g. invisible) therefore holds stale rows the moment any
        // other primitive materializes. The epoch is bumped in exactly those
        // frames so skipped primitives are re-flagged (epoch mismatch) when they
        // next render.
        bool any_material_work = false;
        bool any_possible_runtime_rows_visible = false;

        for (const auto& primitiveComp : primitives)
        {
            if (!primitiveComp)
                continue;

            if (!primitiveComp->IsVisible() || !primitiveComp->CanRender())
                continue;

            const EntityID entity_id = primitiveComp->GetOwnerID();
            if (visibility_storage && visibility_storage->IsInvisible(entity_id))
                continue;

            Entity* entity = primitiveComp->GetOwner();
            if (!entity)
                continue;

            if (!primitiveComp->HasAnyMaterialRecipeSource())
                continue;

            auto material_comp = entity->GetComponent<MaterialComponent>();
            if (!material_comp)
                material_comp = entity->AddComponent<MaterialComponent>();

            const bool runtime_rows =
                material_comp->program
             && graph::mtl::MaterialRequiresRecipeRuntimeRows(
                    material_comp->program->GetShaderResourceSchema());

            // A primitive whose program is not yet resolved may still resolve
            // to a runtime-rows program this frame, so it can trigger a table
            // rebuild. Treat it as a possible runtime-rows primitive.
            const bool possible_runtime_rows =
                runtime_rows || !material_comp->program;

            const bool fast_path_holds =
                   !material_comp->program_dirty
                && material_comp->program
                && material_comp->tracked_material_authored_generation
                   == primitiveComp->GetMaterialAuthoredGeneration()
                && material_comp->resolved_binding_table.IsRuntimeReady();

            const bool epoch_stale =
                runtime_rows
             && material_comp->last_materialize_epoch != materialize_epoch;

            // valid==true only survives a fully successful resolve+prepare+
            // materialize+geometry+pipeline chain, so a Failed material keeps
            // retrying every frame instead of being silently skipped.
            //
            // runtime_dirty is normally cleared at the end of a successful
            // materialize, so at pre-scan time a clean material has it false.
            // It can only be set here if the generation advanced in the middle
            // of the previous frame's loop (a race that leaves it unconsumed)
            // — forcing it into needs_work makes the next frame re-run the full
            // chain so the flag gets consumed.
            const bool needs_work =
                !fast_path_holds || epoch_stale || !material_comp->valid
             || material_comp->runtime_dirty;

            any_material_work |= needs_work;
            any_possible_runtime_rows_visible |= possible_runtime_rows;
        }

        if (any_material_work && any_possible_runtime_rows_visible)
            ++materialize_epoch;

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

                if (!ResolveMaterialProgramForPrimitive(
                            primitiveComp, material_comp))
                {
                    GLogWarning(
                        "[RenderPrimitiveCollectSystem] ResolveMaterialProgramForPrimitive failed for %s",
                        GetPrimitiveOwnerName(primitiveComp));
                    InvalidateRecipeRuntime(material_comp, true);
                    material_comp->MarkFailed();
                }
                else if (!any_material_work
                         && material_comp->last_materialize_epoch == materialize_epoch)
                {
                    // P1-1: all-clean frame — no primitive requires
                    // materialization work, the global tables were last rebuilt
                    // at the current epoch, and this primitive's full chain
                    // succeeded last frame (valid). Everything cached (binding
                    // table, resource plan, materialization rows, runtime
                    // geometry/pipeline) is still valid, so skip the expensive
                    // re-prepare / re-materialize chain. Only the cheap resolve
                    // fast-paths run; on any unexpected failure fall back to
                    // MarkFailed so the next frame retries the full chain.
                    const bool chain_ok =
                        ResolveMaterialProgramForPrimitive(
                            primitiveComp, material_comp)
                     && EnsureRuntimeGeometryFromAsset(
                            world, primitiveComp, material_comp)
                     && ResolveRuntimePipelineForPrimitive(
                            primitiveComp, material_comp);
                    if (chain_ok)
                        material_comp->MarkValid();
                    else
                        material_comp->MarkFailed();
                }
                else
                {
                    material_comp->MarkResourcesPending();
                    const bool resources_ready = material_comp->
                            resolved_binding_table.IsRuntimeReady()
                     && PrepareActivePlanResources(
                            world,
                            primitiveComp,
                            material_comp->program,
                            material_comp->resolved_binding_table);
                    if (!resources_ready)
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] Material resources failed for %s program=%s",
                            GetPrimitiveOwnerName(primitiveComp),
                            material_comp->program
                                ? material_comp->program->
                                    GetName().c_str()
                                : "<null>");
                        InvalidateRecipeRuntime(
                            material_comp, false);
                        material_comp->MarkFailed();
                    }
                    else if (!MaterializeRecipeRowsForPrimitive(
                                primitiveComp, material_comp))
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] MaterializeRecipeRowsForPrimitive failed for %s program=%s",
                            GetPrimitiveOwnerName(primitiveComp),
                            material_comp->program
                                ? material_comp->program->
                                    GetName().c_str()
                                : "<null>");
                        InvalidateRecipeRuntime(
                            material_comp, false);
                        material_comp->MarkFailed();
                    }
                    else if (!EnsureRuntimeGeometryFromAsset(
                                world, primitiveComp, material_comp))
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed for %s",
                            GetPrimitiveOwnerName(primitiveComp));
                        material_comp->MarkFailed();
                    }
                    else if (!ResolveRuntimePipelineForPrimitive(
                                primitiveComp, material_comp))
                    {
                        GLogWarning(
                            "[RenderPrimitiveCollectSystem] ResolveRuntimePipelineForPrimitive failed for %s",
                            GetPrimitiveOwnerName(primitiveComp));
                        material_comp->MarkFailed();
                    }
                    else
                    {
                        material_comp->MarkValid();
                        GLogVerbose(
                            "[DeferredResource] owner=%s valid=%d table=%llu",
                            GetPrimitiveOwnerName(primitiveComp),
                            material_comp->valid ? 1 : 0,
                            static_cast<unsigned long long>(
                                material_comp->resolved_binding_table.
                                    GetStableHash()));
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
