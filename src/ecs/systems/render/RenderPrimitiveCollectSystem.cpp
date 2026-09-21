#include<hgl/ecs/systems/render/RenderPrimitiveCollectSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/support/RenderResource.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/InstancedPrimitiveComponent.h>
#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/core/PrimitiveRenderItem.h>
#include<hgl/ecs/core/InstancedPrimitiveRenderItem.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/tick/VisibilitySystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/graph/mesh/GeometryDataBuffer.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>

#include<hgl/graph/ssbo/MaterialSSBOLayout.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
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

        inline graph::GlobalSSBOType ResolveMaterialSSBORequirementType(
            const graph::mtl::ShaderResourceSlot &req) noexcept
        {
            return req.global_ssbo_type;
        }

        bool BuildResolvedRecipe(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                          const graph::ShaderProgram *material_program,
                                          graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!primitive_comp)
                return false;

            return primitive_comp->BuildResolvedAuthoringMaterialRecipe(out_recipe, material_program);
        }

        const graph::mtl::RecipeTextureBinding *FindRecipeTextureBinding(
            const graph::mtl::MaterialRecipe &recipe,
            const std::string &texture_name) noexcept
        {
            for (const auto &binding : recipe.textures)
            {
                if (binding.texture_name == texture_name)
                    return &binding;
            }
            return nullptr;
        }

        bool ResolveMaterialDefinition(
            const graph::mtl::MaterialRecipe &recipe,
            graph::mtl::MaterialDefinition &out_definition)
        {
            if (!recipe.mtl_def_id.empty()
             && graph::mtl::TryGetMaterialDefinitionByID(
                    recipe.mtl_def_id,
                    out_definition))
                return true;

            return graph::mtl::TryGetMaterialDefinitionByID(
                graph::mtl::GetFallbackMaterialDefinitionID(),
                out_definition);
        }

        bool ValidateMaterialRecipeForRuntime(
            const graph::mtl::MaterialRecipe &recipe,
            const graph::ShaderProgram *material_program,
            const graph::mtl::MaterialDefinition &definition,
            const char *owner_name)
        {
            if (!material_program)
                return false;

            for (size_t i = 0; i < recipe.textures.size(); ++i)
            {
                const auto &binding = recipe.textures[i];
                if (!graph::mtl::IsValidMaterialTextureName(
                        binding.texture_name))
                {
                    GLogError(
                        "[MaterialBinding] Invalid texture name owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        binding.texture_name.c_str());
                    return false;
                }

                if (binding.required && binding.resource_id.empty())
                {
                    GLogError(
                        "[MaterialBinding] Required texture resource missing owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        binding.texture_name.c_str());
                    return false;
                }

                for (size_t j = 0; j < i; ++j)
                {
                    if (recipe.textures[j].texture_name
                            == binding.texture_name)
                    {
                        GLogError(
                            "[MaterialBinding] Duplicate texture binding owner=%s texture=%s",
                            owner_name ? owner_name : "<null>",
                            binding.texture_name.c_str());
                        return false;
                    }
                }

                const int declaration_index =
                    graph::mtl::FindMaterialTextureDeclaration(
                        definition,
                        binding.texture_name);
                if (!definition.texture_declarations.empty()
                 && declaration_index < 0)
                {
                    GLogError(
                        "[MaterialBinding] Undeclared texture owner=%s texture=%s definition=%s",
                        owner_name ? owner_name : "<null>",
                        binding.texture_name.c_str(),
                        definition.definition_id.c_str());
                    return false;
                }

                if (declaration_index >= 0)
                {
                    const auto &declaration =
                        definition.texture_declarations[
                            static_cast<size_t>(declaration_index)];
                    if (binding.array_layer != 0
                     && !graph::mtl::IsMaterialTextureArraySampler(
                            declaration.sampler_type))
                    {
                        GLogError(
                            "[MaterialBinding] Non-array texture received array layer owner=%s texture=%s layer=%u",
                            owner_name ? owner_name : "<null>",
                            binding.texture_name.c_str(),
                            binding.array_layer);
                        return false;
                    }
                }
            }

            for (const auto &declaration : definition.texture_declarations)
            {
                const auto *binding = FindRecipeTextureBinding(
                    recipe,
                    declaration.name);
                if (!binding && declaration.required)
                {
                    GLogError(
                        "[MaterialBinding] Required texture binding missing owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        declaration.name.c_str());
                    return false;
                }

                if (binding
                 && (binding->required || declaration.required)
                 && binding->resource_id.empty())
                {
                    GLogError(
                        "[MaterialBinding] Required texture resource missing owner=%s texture=%s",
                        owner_name ? owner_name : "<null>",
                        declaration.name.c_str());
                    return false;
                }
            }

            for (const auto &req :
                 material_program->GetShaderResourceSchema().resources)
            {
                if (req.semantic
                        != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                    continue;

                const auto &binding = recipe.material_ssbo_binding;
                if (!binding.IsValid()
                 || binding.ssbo_type
                        != ResolveMaterialSSBORequirementType(req))
                {
                    GLogError(
                        "[MaterialBinding] Material data binding missing or invalid owner=%s descriptor=%s type=%s",
                        owner_name ? owner_name : "<null>",
                        req.name.empty() ? "<unnamed>" : req.name.c_str(),
                        graph::GetGlobalSSBOTypeName(
                            ResolveMaterialSSBORequirementType(req)));
                    return false;
                }
            }

            return true;
        }

        bool PrepareActivePlanResources(
            ECSContext *world,
            const std::shared_ptr<PrimitiveComponent> &primitive_comp,
            graph::ShaderProgram *material_program,
            const graph::mtl::MaterialRecipe &active_recipe)
        {
            if (!world || !primitive_comp || !material_program)
                return false;

            graph::mtl::MaterialDefinition definition{};
            if (!ResolveMaterialDefinition(active_recipe, definition)
             || !ValidateMaterialRecipeForRuntime(
                    active_recipe,
                    material_program,
                    definition,
                    GetPrimitiveOwnerName(primitive_comp)))
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

            for (const auto &binding : active_recipe.textures)
            {
                if (binding.resource_id.empty())
                    continue;

                const auto *resource =
                    primitive_comp->GetMaterialTextureResource(
                        binding.texture_name);
                if (!resource
                 || !resource->texture
                 || !resource->sampler
                 || !bindless_mgr)
                {
                    GLogError(
                        "[DeferredResource] Texture acquisition failed: owner=%s texture=%s binding=%d resource=%d bindless=%d",
                        owner_name,
                        binding.texture_name.c_str(),
                        1,
                        resource ? 1 : 0,
                        bindless_mgr ? 1 : 0);
                    return false;
                }

                const std::string resource_id =
                    resource->resource_id.empty()
                        ? BuildTextureResourceId(resource->texture)
                        : resource->resource_id;
                if (resource_id != binding.resource_id)
                {
                    GLogError(
                        "[DeferredResource] Texture identity mismatch: owner=%s texture=%s",
                        owner_name,
                        binding.texture_name.c_str());
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
            && material_comp->tracked_material_authored_generation
                == primitive_comp->GetMaterialAuthoredGeneration())
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
         && material_comp->program)
        {
            // Generation may have advanced without changing the recipe/program
            // content (e.g. an author swapped a texture or data object but kept
            // the same resource id). Refresh the effective recipe as well, so
            // an instance-only data_index change reaches BDA materialization.
            if (material_comp->tracked_material_authored_generation
                != primitive_comp->GetMaterialAuthoredGeneration())
            {
                material_comp->cached_effective_recipe = effective_recipe;
                material_comp->cached_effective_recipe_hash =
                    graph::mtl::HashMaterialRecipe(effective_recipe);
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
        if (!BuildResolvedRecipe(
                primitive_comp,
                resolved_program,
                material_binding_recipe)
         || !ValidateMaterialRecipeForRuntime(
                material_binding_recipe,
                resolved_program,
                template_definition,
                GetPrimitiveOwnerName(primitive_comp)))
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Direct material recipe validation failed for %s",
                GetPrimitiveOwnerName(primitive_comp));
            return false;
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

                const graph::GlobalSSBOType global_ssbo_type =
                    ResolveMaterialSSBORequirementType(req);
                const uint32_t stride = graph::GetGlobalSSBOTypeStructStride(global_ssbo_type);
                if (stride == 0)
                    continue;

                rdbs->RegisterMaterialStructLayout(global_ssbo_type, req.ssbo_id, stride);
            }
        }

        material_comp->program = resolved_program;
        {
            uint32_t planned_textures = 0;
            for (const auto &binding : material_binding_recipe.textures)
                if (!binding.resource_id.empty())
                    ++planned_textures;
            const uint32_t planned_data =
                material_binding_recipe.material_ssbo_binding.IsValid()
                    ? 1u : 0u;
            GLogVerbose(
                "[DeferredResource] owner=%s program=%s planned_texture=%u planned_data=%u recipe_texture=%zu recipe_data=%zu",
                GetPrimitiveOwnerName(primitive_comp),
                resolved_program->GetName().c_str(),
                planned_textures,
                planned_data,
                material_binding_recipe.textures.size(),
                planned_data);
        }
        material_comp->program_dirty = false;
        material_comp->MarkProgramResolved();
        material_comp->recipe_hash = recipe_hash;
        material_comp->program_build_context_hash =
            build_context_hash;

        // effective_recipe 出自 BuildResolvedAuthoringMaterialRecipe（组件边界
        // 已 NormalizeRecipe），直接缓存供 CreatePipeline 使用，无需再规范化。
        material_comp->cached_normalized_recipe = effective_recipe;

        // P3: Cache effective recipe (with program-resolved SSBO types) for
        // direct resource preparation and BDA materialization.
        material_comp->cached_effective_recipe = material_binding_recipe;
        material_comp->cached_effective_recipe_hash =
            graph::mtl::HashMaterialRecipe(material_binding_recipe);

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

        // 当前渲染目标唯一权威：world->GetRenderTarget()（RenderContext 副本已删除；
        // RenderTo 切 RT 时会同步本世界的 render_target 指针）
        auto *render_target = world->GetRenderTarget();
        auto *render_pass = render_target ? render_target->GetRenderPass() : nullptr;
        if (!render_pass)
            return false;

        // 每个 RenderPass 各自有解析好的管线（跨 RT 不互相驱逐）；
        // 该 Pass 已解析过则直接复用。
        if (primitive_comp->HasResolvedRuntimePipeline(render_pass))
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

        primitive_comp->SetResolvedRuntimePipeline(render_pass, resolved_pipeline);
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

        // Consume the normalized, program-resolved recipe directly. It is
        // the source for both texture references and BDA material rows.
        const graph::mtl::MaterialRecipe &effective_recipe =
            material_comp->cached_effective_recipe;
        const graph::mtl::MaterialRecipe &material_binding_recipe =
            material_comp->cached_effective_recipe;
        if (material_comp->cached_effective_recipe_hash == 0)
        {
            GLogWarning(
                "[RenderPrimitiveCollectSystem] Materialize failed: effective material recipe is not cached for %s",
                GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        if (getenv("ULRE_ARENA_DEBUG"))
            GLogInfo("[ArenaTrace] materialize entry: material_binding_valid=%d schema_reqs=%u",
                     material_binding_recipe.material_ssbo_binding.IsValid() ? 1 : 0,
                     (uint32_t)material_comp->program->GetShaderResourceSchema().resources.size());

        // Keep the schema-to-recipe readiness check. The recipe binding below is
        // the single source for the BDA row address and active data ID.
        for (const auto &req : material_comp->program->GetShaderResourceSchema().resources)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialPrivateData)
                continue;

            const auto &recipe_binding =
                material_binding_recipe.material_ssbo_binding;
            if (!recipe_binding.IsValid()
             || recipe_binding.ssbo_type
                    != ResolveMaterialSSBORequirementType(req))
            {
                GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: unresolved SSBO binding for %s descriptor=%s type=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            req.name.empty() ? "<unnamed>" : req.name.c_str(),
                            graph::GetGlobalSSBOTypeName(
                                ResolveMaterialSSBORequirementType(req)));
                return false;
            }
        }

        auto rdbs = world->GetSystem<RenderSceneUBOSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: RenderSceneUBOSystem missing for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        // The binding recipe carries the primitive's active material row ID.
        const auto &asset_binding =
            material_binding_recipe.material_ssbo_binding;
        const uint32_t entity_data_index =
            asset_binding.IsValid() ? asset_binding.data_index : uint32_t(-1);

        // Fill the per-batch material address row for the shared material SSBO.
        // 每个材质 recipe 只声明一个共享材质数据 SSBO。
        if (asset_binding.IsValid())
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
                             1u);
                }

                material_comp->material_row_cpu = nullptr;
                material_comp->material_row_gpu     = 0;

                auto *graphics_context = world->GetGraphicsContext();
                auto *material_domain = graphics_context
                    ? graphics_context->GetGlobalSSBOBufferRegistry()
                    : nullptr;
                graph::GlobalRowBufferInfo material_buffer{};
                if (!material_domain
                 || !material_domain->TryGetRowBuffer(
                        asset_binding.ssbo_id,
                        material_buffer))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: material row buffer missing for %s type=%s ssbo_id=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::GetGlobalSSBOTypeName(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id);
                    return false;
                }
                if (!material_domain->IsActive(
                        asset_binding.ssbo_type,
                        asset_binding.data_index))
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: inactive material row ID for %s type=%s ssbo_id=%u data_index=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::GetGlobalSSBOTypeName(
                            asset_binding.ssbo_type),
                        asset_binding.ssbo_id,
                        asset_binding.data_index);
                    return false;
                }
                if (material_buffer.global_ssbo_type
                        != asset_binding.ssbo_type
                 || material_buffer.gpu_base == 0
                 || material_buffer.row_bytes == 0
                 || material_buffer.row_capacity == 0
                 || asset_binding.data_index >= material_buffer.row_capacity)
                {
                    GLogError(
                        "[RenderPrimitiveCollectSystem] Materialize failed: material row buffer invalid for %s type=%s buffer_type=%s ssbo_id=%u data_index=%u capacity=%u row_bytes=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        graph::GetGlobalSSBOTypeName(
                            asset_binding.ssbo_type),
                        graph::GetGlobalSSBOTypeName(
                            material_buffer.global_ssbo_type),
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

            //if (getenv("ULRE_ARENA_DEBUG"))
            //{
            //    GLogInfo(
            //        "[MaterialTextureReferences] owner=%s definition=%s row=%u references=%u gpu=0x%llx",
            //        GetPrimitiveOwnerName(primitive_comp),
            //        texture_definition.definition_id.c_str(),
            //        new_allocation.row_index,
            //        texture_layout.reference_count,
            //        static_cast<unsigned long long>(
            //            new_allocation.gpu_row));
            //    for (size_t i = 0;
            //         i < texture_definition.texture_declarations.size();
            //         ++i)
            //    {
            //        const auto &declaration =
            //            texture_definition.texture_declarations[i];
            //        const auto &reference =
            //            references[static_cast<int>(i)];
            //        GLogInfo(
            //            "[MaterialTextureReferences] texture=%s descriptor=%u layer=%u",
            //            declaration.name.c_str(),
            //            reference.descriptor_index,
            //            reference.array_layer);
            //    }
            //}
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
        // (rows are rebuilt from the recipe bindings / texture layer values
        // each time a primitive is materialized). A primitive skipped
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
            // to a runtime-rows program this frame, so it can trigger direct
            // recipe materialization. Treat it as a possible runtime-rows
            // primitive.
            const bool possible_runtime_rows =
                runtime_rows || !material_comp->program;

            const bool fast_path_holds =
                   !material_comp->program_dirty
                && material_comp->program
                && material_comp->tracked_material_authored_generation
                   == primitiveComp->GetMaterialAuthoredGeneration()
                && material_comp->cached_effective_recipe_hash != 0;

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
                    // materialization work, this primitive's full chain
                    // succeeded last frame (valid), and the current
                    // materialization epoch is already covered. Everything
                    // cached (resource preparation, materialization rows,
                    // runtime geometry/pipeline) is still valid, so skip the
                    // expensive re-prepare / re-materialize chain. Only the
                    // cheap resolve fast-paths run; on any unexpected failure
                    // fall back to MarkFailed so the next frame retries the
                    // full chain.
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
                    const bool resources_ready =
                        PrepareActivePlanResources(
                            world,
                            primitiveComp,
                            material_comp->program,
                            material_comp->cached_effective_recipe);
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
                            "[DeferredResource] owner=%s valid=%d",
                            GetPrimitiveOwnerName(primitiveComp),
                            material_comp->valid ? 1 : 0);
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

            // ── 同步 4-ID 描述符至 PrimitiveComponent 与 RenderItemDataStorage ──
            const uint32_t transform_id = transform->GetStorageHandle();
            uint32_t geometry_id = 0;
            const auto *geom_buf = primitiveComp->GetRuntimeGeometryDataBuffer();
            if (geom_buf)
            {
                geometry_id = geom_buf->geometry_id;
            }
            if (geometry_id == 0 && primitiveComp->GetPrimitiveAsset())
            {
                if (auto *geom = primitiveComp->GetPrimitiveAsset()->GetGeometry())
                {
                    auto *gc = world ? world->GetGraphicsContext() : nullptr;
                    auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
                    auto *dev = world ? world->GetGPUDevice() : nullptr;
                    if (pool && dev)
                    {
                        const_cast<graph::Geometry *>(geom)->EnsureMeshDrawParams(pool, dev);
                    }
                    geometry_id = geom->GetGeometryID();
                    if (geom_buf)
                    {
                        const_cast<graph::GeometryDataBuffer *>(geom_buf)->geometry_id = geometry_id;
                    }
                }
            }
            const uint32_t material_id = (material_for_item && material_for_item->data_index_row != uint32_t(-1))
                ? material_for_item->data_index_row : 0;
            const uint32_t texture_id = (material_for_item && material_for_item->material_texture_configuration.IsValid())
                ? material_for_item->material_texture_configuration.row_index : 0;

            std::unique_ptr<PrimitiveRenderItem> item;

            if (auto instancedComp = std::dynamic_pointer_cast<InstancedPrimitiveComponent>(primitiveComp))
            {
                if (instancedComp->GetAllocatedInstanceCapacity() > 1)
                {
                    instancedComp->SetAllInstances4ID(transform_id, geometry_id, material_id, texture_id, false);
                }
                else
                {
                    primitiveComp->Set4ID(transform_id, geometry_id, material_id, texture_id);
                }

                item = std::make_unique<InstancedPrimitiveRenderItem>(
                    entity_id, transform, instancedComp, material_for_item, world);
            }
            else
            {
                primitiveComp->Set4ID(transform_id, geometry_id, material_id, texture_id);
                item = std::make_unique<PrimitiveRenderItem>(
                    entity_id, transform, primitiveComp, material_for_item, world);
            }

            glm::vec3 worldPos = transform->GetWorldPosition();
            item->worldPosition = worldPos;
            glm::vec3 toCamera = worldPos - camera_pos;
            item->distanceToCamera = glm::length(toCamera);

            item->UpdateWorldMatrix();

            cache.renderItems.push_back(std::move(item));
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
