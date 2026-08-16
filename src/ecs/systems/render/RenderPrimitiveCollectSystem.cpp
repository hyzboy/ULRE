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
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/BindingTableBuilder.h>
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
                GLogError("[RenderPrimitiveCollectSystem] EnsureRuntimeGeometryFromAsset failed: material program null owner=%s valid=%d program_dirty=%d runtime_dirty=%d",
                          GetPrimitiveOwnerName(primitive_comp),
                          material_comp->valid ? 1 : 0,
                          material_comp->program_dirty ? 1 : 0,
                          material_comp->runtime_dirty ? 1 : 0);
                return false;
            }
            return primitive_comp->EnsureRuntimeGeometryBinding(material);
        }

        bool ResolveRecipeSSBOBindingId(const graph::mtl::MaterialRecipe &recipe,
                                        const graph::mtl::ShaderResourceSlot &req,
                                        uint32_t &out_ssbo_id)
        {
            if (const auto *asset = graph::mtl::FindRecipeSSBOAssetBinding(
                    recipe, req.name.c_str(), req.data_slot, req.ssbo_type))
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
                    "[MaterialBinding][MissingTexture] view_index=%d logical=%llu slot=%u source=%s required=%d allow_fallback=%d recipe_index=%u asset_hash=%llu metadata_hash=%llu direct=%u",
                    i,
                    static_cast<unsigned long long>(
                        binding.logical_resource_id),
                    static_cast<uint32_t>(binding.texture_slot),
                    graph::mtl::GetBindingSourceName(
                        binding.source),
                    binding.required ? 1 : 0,
                    binding.allow_fallback ? 1 : 0,
                    binding.recipe_binding_index,
                    static_cast<unsigned long long>(
                        binding.asset_identity_hash),
                    static_cast<unsigned long long>(
                        binding.asset_metadata_hash),
                    binding.direct_value);
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
                    binding.data_slot,
                    graph::mtl::GetSSBOTypeName(binding.ssbo_type),
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
                    "[MaterialBinding][RecipeTexture] index=%zu slot=%s resource=%s direct=%d direct_value=%u required=%d",
                    i,
                    binding.slot_name.c_str(),
                    binding.resource_id.c_str(),
                    binding.use_direct_value ? 1 : 0,
                    binding.direct_value,
                    binding.required ? 1 : 0);
            }
            for (size_t i = 0; i < recipe.ssbo_assets.size(); ++i)
            {
                const auto &binding = recipe.ssbo_assets[i];
                GLogWarning(
                    "[MaterialBinding][RecipeData] index=%zu name=%s slot=%u type=%s(%u) ssbo_id=%u data_index=%u use_data_index=%d shared=%d",
                    i,
                    binding.data_slot_name.c_str(),
                    binding.data_slot,
                    graph::mtl::GetSSBOTypeName(binding.ssbo_type),
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
                            MaterialDataSlotData)
                    continue;
                GLogWarning(
                    "[MaterialBinding][Layout] index=%zu name=%s semantic=%s kind=%s required=%d allow_fallback=%d texture_slot=%u data_slot=%u type=%s(%u) ssbo_id=%u",
                    i,
                    requirement.name.empty() ? "<unnamed>" : requirement.name.c_str(),
                    graph::mtl::GetDescriptorSemanticName(
                        requirement.semantic),
                    graph::mtl::GetDescriptorKindName(requirement.kind),
                    requirement.required ? 1 : 0,
                    requirement.allow_fallback ? 1 : 0,
                    static_cast<uint32_t>(requirement.texture_slot),
                    requirement.data_slot,
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

            auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
            auto *render_context = world->GetRenderContext();
            auto *graphics_context = render_context
                ? render_context->GetGraphicsContext()
                : world->GetGraphicsContext();
            auto *domain_manager = graphics_context
                ? graphics_context->GetResourceDomainManager() : nullptr;
            auto *bindless_mgr = graphics_context
                ? graphics_context->
                    GetManager<graph::BindlessTextureManager>()
                : nullptr;
            if (!rdbs || !domain_manager)
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
                    if (candidate.slot_name == graph::mtl::GetTextureSlotName(binding.texture_slot)
                     && !candidate.use_direct_value
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
                        binding.texture_slot);
                if (!recipe_binding
                 || !resource
                 || resource->use_direct_value
                 || !bindless_mgr)
                {
                    GLogError(
                        "[DeferredResource] Texture acquisition failed: owner=%s slot=%u recipe=%d resource=%d bindless=%d",
                        owner_name,
                        static_cast<uint32_t>(
                            binding.texture_slot),
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
                        "[DeferredResource] Texture identity mismatch: owner=%s slot=%u",
                        owner_name,
                        static_cast<uint32_t>(
                            binding.texture_slot));
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
                    if (candidate.data_slot == binding.data_slot
                     && candidate.ssbo_type == binding.ssbo_type
                     && graph::mtl::GetResolvedDataAssetIdentityHash(
                            candidate.ssbo_type,
                            candidate.ssbo_id,
                            candidate.data_slot)
                            == binding.asset_identity_hash)
                    {
                        recipe_binding = &candidate;
                    }
                }
                if (!recipe_binding)
                    return false;

                const graph::mtl::ShaderResourceSlot
                    *layout_requirement = nullptr;
                for (const auto &requirement :
                     material_program->
                        GetShaderResourceSchema().resources)
                {
                    if (requirement.semantic
                            == graph::mtl::DescriptorSemantic::
                                MaterialDataSlotData
                     && requirement.data_slot == binding.data_slot
                     && requirement.ssbo_type == binding.ssbo_type)
                    {
                        layout_requirement = &requirement;
                        break;
                    }
                }
                if (!layout_requirement || layout_requirement->name.empty())
                    return false;

                const graph::mtl::SSBOAddress address{
                    binding.ssbo_type,
                    recipe_binding->ssbo_id,
                    0};
                graph::ResourceDomainBinding domain_binding{};
                if (!domain_manager->TryGetBinding(
                        address, domain_binding)
                 || !domain_binding.buffer
                 || domain_binding.element_stride == 0)
                {
                    const auto *resource =
                        primitive_comp->GetMaterialDataSlotResource(
                            layout_requirement->name,
                            binding.data_slot);
                    if (!resource
                     || !resource->buffer
                     || resource->ssbo_id != recipe_binding->ssbo_id)
                        return false;

                    if (!rdbs->RegisterMaterialStructLayout(
                            binding.ssbo_type,
                            recipe_binding->ssbo_id,
                            resource->byte_stride)
                     || !domain_manager->RegisterBuffer(
                            address,
                            resource->buffer,
                            resource->element_capacity))
                        return false;

                    if (!domain_manager->TryGetBinding(
                            address, domain_binding)
                     || !domain_binding.buffer
                     || domain_binding.element_stride == 0)
                        return false;
                }

                if (!rdbs->RegisterMaterialStructLayout(
                        binding.ssbo_type,
                        recipe_binding->ssbo_id,
                        domain_binding.element_stride))
                    return false;
            }
            return true;
        }

        void InvalidateRecipeRuntime(const std::shared_ptr<MaterialComponent> &material_comp,
                                     const bool clear_program)
        {
            if (!material_comp)
                return;

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
        graph::shadergen::ShaderProgramPurpose effective_purpose =
            graph::shadergen::ShaderProgramPurpose::ForwardColor;
        switch (primitive_comp->GetPrimitiveVariantPurpose())
        {
        case graph::PrimitiveVariantPurpose::DepthOnly:
            effective_purpose =
                graph::shadergen::ShaderProgramPurpose::DepthOnly;
            break;
        case graph::PrimitiveVariantPurpose::ShadowCaster:
            effective_purpose =
                graph::shadergen::ShaderProgramPurpose::ShadowDepth;
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

        // 统一 MaterialDefinition 入口：由 AcquireShaderProgram 内部处理 2D/3D/Text/Sky 分支，
        // ECS 不再持有材质 config 细节知识。
        graph::mtl::MaterialDefinitionBuildRequest mtl_request{};
        mtl_request.recipe = effective_recipe;
        mtl_request.primitive_type = primitive_type;
        mtl_request.geometry_vertex_format = geometry_vertex_format;
        if (effective_purpose
            != graph::shadergen::ShaderProgramPurpose::ForwardColor)
        {
            mtl_request.override_shader_program_purpose = true;
            mtl_request.shader_program_purpose = effective_purpose;
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

        if (auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            for (const auto &req : resolved_program->GetShaderResourceSchema().resources)
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

        // Cache normalized recipe so CreatePipeline can skip duplicate NormalizeRecipe.
        {
            graph::mtl::MaterialRecipe cached_recipe = effective_recipe;
            graph::mtl::NormalizeRecipe(cached_recipe);
            material_comp->cached_normalized_recipe = std::move(cached_recipe);
        }

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

        if (!material_comp->program
         || !graph::mtl::MaterialRequiresRecipeRuntimeRows(
                material_comp->program->GetShaderResourceSchema()))
        {
            material_comp->data_index_row = 0;
            material_comp->data_index_values.clear();
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

        material_comp->ClearResolvedSSBOBindings();
        material_comp->data_index_values.clear();
        for (const auto &req : material_comp->program->GetShaderResourceSchema().resources)
        {
            if (req.semantic != graph::mtl::DescriptorSemantic::MaterialDataSlotData)
                continue;

            uint32_t resolved_ssbo_id = 0;
            if (!ResolveRecipeSSBOBindingId(
                    material_binding_recipe, req, resolved_ssbo_id))
            {
                GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: unresolved SSBO binding for %s descriptor=%s slot=%u type=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            req.name.empty() ? "<unnamed>" : req.name.c_str(),
                            req.data_slot,
                            graph::mtl::GetSSBOTypeName(req.ssbo_type));
                return false;
            }

            material_comp->SetResolvedSSBOBinding(
                req.name.c_str(), req.data_slot, req.ssbo_type, resolved_ssbo_id);
        }

        auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: RenderDescriptorBindingSystem missing for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        // Determine the entity's own data_index from the cached binding recipe.
        // The old shared MaterializationSpec cache is gone: this value is always
        // this primitive's own row, never inherited from another primitive.
        //
        // A use_data_index == false asset (e.g. every mesh in BasicLitMeshes /
        // TextureBlinnPhongMeshes) still owns a concrete row: the shader indexes
        // mtl_data_index_rows and mtl_texture_layer_rows by the data_index VALUE
        // read back from those tables, so a non-data-index asset publishes its
        // authored data_index (0) there. Prefer an explicit use_data_index asset
        // when present, otherwise fall back to the first authored data_index.
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

        // Single source of truth for the texture-layer rows scope: the bind
        // side (resolve_recipe_batch_struct_ssbo_id) reads back exactly this
        // (name, data_slot, ssbo_type) triple from the resolved bindings.
        material_comp->SetResolvedSSBOBinding(
            "mtl_texture_layer_rows",
            graph::mtl::DefaultMaterialDataSlot,
            graph::mtl::SSBOType::TextureLayer,
            scope_ssbo_id);

        // Fill the per-batch material data index table for every SSBO asset,
        // including use_data_index == false ones (the shader still reads
        // data[data_index], so the authored index must be published in the table).
        for (const auto &asset_binding : material_binding_recipe.ssbo_assets)
        {
            uint32_t data_slot = asset_binding.data_slot;
            for (const auto &req : material_comp->program->GetShaderResourceSchema().resources)
            {
                if (req.semantic == graph::mtl::DescriptorSemantic::MaterialDataSlotData
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

        // The texture-layer row is keyed by the primitive's data_index VALUE.
        // Publish the row even when the primitive authors no data slot: it
        // still owns row 0, and its data_index resolves to 0 through
        // mtl_data_index_rows, so bindless lookups stay aligned.
        material_comp->data_index_row =
            entity_data_index != uint32_t(-1) ? entity_data_index : 0u;
        const uint32_t texture_layer_row = material_comp->data_index_row;

        // Build this primitive's texture layer row from the single binding IR
        // and write it into the engine-managed domain SSBO keyed by
        // scope_ssbo_id. The row layout is TextureLayerRowsData (10 uints,
        // handle stored at the TextureSlot enum index) — byte-identical to the
        // pre-p1-2d-3 flat values[RANGE_SIZE] layout.
        uint32_t row_data[static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE)] = {};

        for (const auto &texture_binding : material_binding_recipe.textures)
        {
            graph::mtl::TextureSlot slot_enum;
            if (!graph::mtl::ParseTextureSlotName(texture_binding.slot_name, slot_enum))
                continue;
            const uint32_t slot = static_cast<uint32_t>(slot_enum);

            uint32_t handle = 0;
            if (texture_binding.use_direct_value)
            {
                handle = texture_binding.direct_value;
            }
            else
            {
                handle = rdbs->GetBindlessHandle(
                    AnsiString(texture_binding.resource_id.c_str()));

                if (handle == 0)
                {
                    // The recipe's resource id may be empty; fall back to the
                    // authored resource id, or the texture-derived id used at
                    // RegisterTexture2D(Array)Resource time.
                    if (const auto *authoring =
                            primitive_comp->GetMaterialTextureResource(slot_enum))
                    {
                        if (!authoring->use_direct_value && authoring->texture)
                        {
                            const std::string fallback_id =
                                authoring->resource_id.empty()
                                    ? BuildTextureResourceId(authoring->texture)
                                    : authoring->resource_id;
                            handle = rdbs->GetBindlessHandle(AnsiString(fallback_id.c_str()));
                        }
                    }
                }
            }

            if (handle == 0 && !texture_binding.use_direct_value)
            {
                GLogWarning("[RenderPrimitiveCollectSystem] materialize: bindless handle missing for %s slot=%u resource=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            slot,
                            texture_binding.resource_id.empty() ? "<unnamed>" : texture_binding.resource_id.c_str());
            }

            if (slot < static_cast<uint32_t>(graph::mtl::TextureSlot::RANGE_SIZE))
                row_data[slot] = handle;
        }

        auto *render_context = world->GetRenderContext();
        auto *graphics_context = render_context
            ? render_context->GetGraphicsContext()
            : world->GetGraphicsContext();
        auto *domain_manager = graphics_context
            ? graphics_context->GetResourceDomainManager() : nullptr;

        if (domain_manager)
        {
            const VkDeviceSize stride =
                graph::mtl::GetSSBOTypeStructStride(graph::mtl::SSBOType::TextureLayer);
            graph::DeviceBuffer *domain_buffer = domain_manager->EnsureBuffer(
                graph::mtl::SSBOAddress{graph::mtl::SSBOType::TextureLayer, scope_ssbo_id, 0},
                "mtl_texture_layer_rows",
                stride * static_cast<VkDeviceSize>(texture_layer_row + 1),
                texture_layer_row + 1);
            if (domain_buffer)
            {
                auto *gpu = domain_buffer->GetGPUBuffer();
                if (gpu)
                    gpu->Write(row_data, static_cast<VkDeviceSize>(texture_layer_row) * stride, stride);
            }
            else
            {
                GLogWarning("[RenderPrimitiveCollectSystem] materialize: domain texture layer rows buffer missing for %s scope_ssbo_id=%u row=%u",
                            GetPrimitiveOwnerName(primitive_comp),
                            scope_ssbo_id,
                            texture_layer_row);
            }
        }
        else
        {
            GLogWarning("[RenderPrimitiveCollectSystem] materialize: domain manager missing, texture layer rows not written for %s",
                        GetPrimitiveOwnerName(primitive_comp));
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
