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
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/log/Log.h>
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
                return false;

            const auto *asset = primitive_comp->GetPrimitiveAsset();
            if (!asset)
                return true;

            auto *material = material_comp->program;
            if (!material)
                return false;
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
                    if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                        continue;

                    for (const auto &ref : spec.struct_refs)
                    {
                        if (ref.slot_index == req.slot_index && ref.ssbo_type == req.ssbo_type)
                            return ref.struct_index;
                    }
                }
            }

            if (!spec.struct_refs.empty())
                return spec.struct_refs.front().struct_index;

            return fallback_row;
        }

        void UpsertRecipeTextureBinding(graph::mtl::MaterialRecipe &recipe,
                                        graph::mtl::TextureSlot slot,
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

            graph::mtl::RecipeTextureBinding binding{};
            binding.slot = slot;
            binding.resource_id = resource_id;
            binding.direct_value = direct_value;
            binding.use_direct_value = use_direct_value;
            binding.required = required;
            recipe.textures.emplace_back(std::move(binding));
        }

        void UpsertRecipeStructBinding(graph::mtl::MaterialRecipe &recipe,
                                       graph::mtl::SSBOType ssbo_type,
                                       const uint32_t ssbo_id,
                                       const uint32_t struct_index,
                                       const bool use_struct_index,
                                       const bool shared_across_instances)
        {
            const uint32_t slot_index = graph::mtl::GetMaterialStructSlotIndex(ssbo_type);
            for (auto &binding : recipe.structs)
            {
                if (binding.slot_index != slot_index || binding.ssbo_type != ssbo_type)
                    continue;

                binding.ssbo_type = ssbo_type;
                binding.ssbo_id = ssbo_id;
                binding.struct_index = struct_index;
                binding.use_struct_index = use_struct_index;
                binding.shared_across_instances = shared_across_instances;
                return;
            }

            graph::mtl::RecipeStructBinding binding{};
            binding.slot_index = slot_index;
            binding.ssbo_type = ssbo_type;
            binding.ssbo_id = ssbo_id;
            binding.struct_index = struct_index;
            binding.use_struct_index = use_struct_index;
            binding.shared_across_instances = shared_across_instances;
            recipe.structs.emplace_back(std::move(binding));
        }

        void NormalizeRecipeWithBaseMaterialInfo(graph::mtl::MaterialRecipe &recipe)
        {
            if (recipe.mtl_def_id.empty())
                return;

            graph::mtl::MaterialDefinition bmi{};
            if (graph::mtl::TryGetMaterialDefinitionByID(recipe.mtl_def_id, bmi))
                graph::mtl::ApplyBaseMaterialInfoDefaults(recipe, bmi, false);

            // Auto-derive SSBO name mapping from struct slot declarations when authoring didn't provide assets.
            if (recipe.ssbo_assets.empty())
            {
                for (const auto &binding : recipe.structs)
                    graph::mtl::UpsertRecipeSSBOAssetBinding(recipe, "mtl", graph::mtl::SSBOBinding{binding.ssbo_type, binding.ssbo_id});
            }
        }

        bool BuildEffectiveMaterialRecipe(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                          graph::mtl::MaterialRecipe &out_recipe)
        {
            if (!primitive_comp)
                return false;

            const auto *base_recipe = primitive_comp->GetMaterialRecipe();
            if (!base_recipe)
                return false;

            out_recipe = *base_recipe;

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE); ++i)
            {
                const auto slot = static_cast<graph::mtl::TextureSlot>(i);
                const auto *resource = primitive_comp->GetMaterialTextureResource(slot);
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
                    // GLogInfo("[TexTrace] BuildEffectiveRecipe slot=%zu direct_value=%u", i, resource->direct_value);
                    continue;
                }

                const std::string resource_id = resource->resource_id.empty()
                                              ? BuildTextureResourceId(resource->texture)
                                              : resource->resource_id;
                if (resource_id.empty())
                {
                    GLogWarning("[TexTrace] BuildEffectiveRecipe slot=%zu texture=%p resource_id EMPTY (skip)", i, (void*)resource->texture);
                    continue;
                }

                // GLogInfo("[TexTrace] BuildEffectiveRecipe slot=%zu resource_id=%s", i, resource_id.c_str());
                UpsertRecipeTextureBinding(out_recipe, slot, resource_id, resource->required);
            }

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::MaterialStructSlotCount); ++i)
            {
                const auto ssbo_type = graph::mtl::GetMaterialStructSSBOTypeBySlotIndex(static_cast<uint32_t>(i));
                const auto *resource = primitive_comp->GetMaterialStructResource(ssbo_type);
                if (!resource)
                    continue;

                UpsertRecipeStructBinding(out_recipe,
                                         resource->ssbo_type,
                                         resource->ssbo_id,
                                         resource->struct_index,
                                         resource->use_struct_index,
                                         resource->shared_across_instances);
            }

            // GLogInfo("[TexTrace] BuildEffectiveRecipe result: recipe=%s tex_bindings=%zu struct_bindings=%zu",
            //          out_recipe.recipe_name.c_str(), out_recipe.textures.size(), out_recipe.structs.size());

            NormalizeRecipeWithBaseMaterialInfo(out_recipe);

            return true;
        }

        bool PrepareRecipeAuthoringResources(ECSContext *world,
                                             const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                             graph::ShaderProgram *material_program)
        {
            if (!world || !primitive_comp)
                return false;

            graph::mtl::MaterialRecipe effective_recipe{};
            if (!BuildEffectiveMaterialRecipe(primitive_comp, effective_recipe))
                return false;

            auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
            auto *render_context = world->GetRenderContext();
            auto *graphics_context = render_context ? render_context->GetGraphicsContext() : world->GetGraphicsContext();
            auto *domain_manager = graphics_context ? graphics_context->GetResourceDomainManager() : nullptr;
            auto *bindless_mgr = graphics_context ? graphics_context->GetManager<graph::BindlessTextureManager>() : nullptr;
            if (!rdbs || !domain_manager)
                return false;

            const char *prim_owner = GetPrimitiveOwnerName(primitive_comp);
            // GLogInfo("[TexTrace] PrepareRecipeAuthoringResources for %s bindless_mgr=%p", prim_owner, (void*)bindless_mgr);

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::TextureSlot::RANGE_SIZE); ++i)
            {
                const auto slot = static_cast<graph::mtl::TextureSlot>(i);
                const auto *resource = primitive_comp->GetMaterialTextureResource(slot);
                if (!resource)
                {
                    // GLogInfo("[TexTrace]   slot=%zu: no resource", i);
                    continue;
                }

                if (resource->use_direct_value)
                {
                    // GLogInfo("[TexTrace]   slot=%zu: direct_value=%u (skip bindless)", i, resource->direct_value);
                    continue;
                }

                const std::string resource_id = resource->resource_id.empty()
                                              ? BuildTextureResourceId(resource->texture)
                                              : resource->resource_id;
                if (resource_id.empty() || !bindless_mgr)
                {
                    GLogError("[TexTrace]   slot=%zu: FAIL resource_id_empty=%d bindless_mgr_null=%d", i, resource_id.empty()?1:0, !bindless_mgr?1:0);
                    return false;
                }

                uint32_t handle = 0;
                switch (resource->kind)
                {
                    case PrimitiveComponent::MaterialTextureResourceKind::Texture2D:
                        handle = rdbs->RegisterTexture2DResource(resource_id, resource->texture, resource->sampler, bindless_mgr);
                        // GLogInfo("[TexTrace]   slot=%zu Texture2D resource_id=%s handle=%u", i, resource_id.c_str(), handle);
                        break;
                    case PrimitiveComponent::MaterialTextureResourceKind::Texture2DArray:
                        handle = rdbs->RegisterTexture2DArrayResource(resource_id, resource->texture, resource->sampler, bindless_mgr);
                        // GLogInfo("[TexTrace]   slot=%zu Texture2DArray resource_id=%s handle=%u", i, resource_id.c_str(), handle);
                        break;
                    default:
                        GLogWarning("[TexTrace]   slot=%zu unknown kind=%d", i, (int)resource->kind);
                        break;
                }

                if (handle == 0)
                {
                    GLogError("[TexTrace]   slot=%zu: RegisterTexture returned handle=0, FAIL", i);
                    return false;
                }

                if (!material_program)
                {
                    // GLogInfo("[TexTrace]   slot=%zu: no material_program, skip per-descriptor register", i);
                    continue;
                }

                for (const auto &req : material_program->GetMaterialResourceLayout().requirements)
                {
                    if (req.texture_slot != slot || !req.name || !*req.name)
                        continue;

                    switch (req.semantic)
                    {
                        case graph::mtl::DescriptorSemantic::MaterialTexture:
                            // GLogInfo("[TexTrace]   slot=%zu: RegisterMaterialTexture descriptor=%s", i, req.name);
                            if (!rdbs->RegisterMaterialTexture(material_program, req.name, resource->texture))
                            {
                                GLogError("[TexTrace]   RegisterMaterialTexture FAILED descriptor=%s", req.name);
                                return false;
                            }
                            break;
                        case graph::mtl::DescriptorSemantic::MaterialSampler:
                            // GLogInfo("[TexTrace]   slot=%zu: RegisterMaterialTextureSampler descriptor=%s", i, req.name);
                            if (!rdbs->RegisterMaterialTextureSampler(material_program, req.name, resource->texture, resource->sampler))
                            {
                                GLogError("[TexTrace]   RegisterMaterialTextureSampler FAILED descriptor=%s", req.name);
                                return false;
                            }
                            break;
                        default:
                            break;
                    }
                }
            }

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::MaterialStructSlotCount); ++i)
            {
                const auto ssbo_type = graph::mtl::GetMaterialStructSSBOTypeBySlotIndex(static_cast<uint32_t>(i));
                const auto *resource = primitive_comp->GetMaterialStructResource(ssbo_type);
                if (!resource)
                    continue;

                const graph::mtl::SSBOAddress address{resource->ssbo_type, resource->ssbo_id, 0};
                if (resource->buffer)
                {
                    if (!rdbs->RegisterMaterialStructLayout(resource->ssbo_type, resource->ssbo_id, resource->byte_stride))
                        return false;

                    if (!domain_manager->RegisterBuffer(address, resource->buffer, resource->element_capacity))
                        return false;
                }
                else
                {
                    graph::ResourceDomainBinding binding{};
                    if (!domain_manager->TryGetBinding(address, binding) || !binding.buffer || binding.element_stride == 0)
                        return false;

                    if (!rdbs->RegisterMaterialStructLayout(resource->ssbo_type, resource->ssbo_id, binding.element_stride))
                        return false;
                }
            }

            if (!material_program)
                return true;

            for (const auto &req : material_program->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                const auto *asset_binding = graph::mtl::FindRecipeSSBOAssetBinding(effective_recipe, req.name, req.ssbo_type);
                const auto *named_struct_resource = primitive_comp->GetMaterialStructResource(std::string(req.name ? req.name : ""));

                if (!asset_binding && named_struct_resource)
                {
                    graph::mtl::UpsertRecipeSSBOAssetBinding(effective_recipe,
                                                             req.name ? std::string(req.name) : std::string(),
                                                             req.ssbo_type,
                                                             named_struct_resource->ssbo_id);

                    asset_binding = graph::mtl::FindRecipeSSBOAssetBinding(effective_recipe, req.name, req.ssbo_type);
                }

                if (!primitive_comp->GetMaterialStructResource(req.ssbo_type) && named_struct_resource)
                {
                    primitive_comp->SetMaterialStructResource(req.ssbo_type,
                                                              named_struct_resource->ssbo_id,
                                                              nullptr,
                                                              0,
                                                              0,
                                                              named_struct_resource->struct_index,
                                                              named_struct_resource->use_struct_index,
                                                              named_struct_resource->shared_across_instances);
                }

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

            material_comp->material_instance_row = uint32_t(-1);
            material_comp->texture_layer_row = uint32_t(-1);
            material_comp->data_index_row = uint32_t(-1);
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
        if (!BuildEffectiveMaterialRecipe(primitive_comp, effective_recipe))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] BuildEffectiveMaterialRecipe failed for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        const uint64_t recipe_hash = graph::mtl::HashMaterialRecipe(effective_recipe);
        if (material_comp->recipe_hash != recipe_hash)
        {
            material_comp->program_dirty = true;
            InvalidateRecipeRuntime(material_comp, false);
        }

        if (!material_comp->program_dirty && material_comp->program)
            return true;

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

        // 统一 BMI 入口：由 AcquireMaterialProgram 内部处理 2D/3D/Text/Sky 分支，
        // ECS 不再持有材质 config 细节知识。
        graph::ShaderProgram *resolved_program =
            material_manager->AcquireMaterialProgram(
                effective_recipe.mtl_def_id,
                effective_recipe,
                primitive_type,
                geometry_vertex_format);

        if (!resolved_program)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] AcquireMaterialProgram failed for %s recipe=%s mtl_def_id=%s builtin_creator_id=%u",
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
            const uint32_t mi_data_bytes = resolved_program->GetMIDataBytes();
            if (mi_data_bytes > 0)
            {
                for (const auto &req : resolved_program->GetMaterialResourceLayout().requirements)
                {
                    if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                        continue;

                    rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes);
                }
            }
        }

        material_comp->program = resolved_program;
        material_comp->program_dirty = false;
        material_comp->recipe_hash = recipe_hash;
        return true;
    }

    bool RenderPrimitiveCollectSystem::MaterializeRecipeRowsForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                                         const std::shared_ptr<MaterialComponent> &material_comp)
    {
        if (!world || !primitive_comp || !material_comp)
            return false;

        graph::mtl::MaterialRecipe effective_recipe{};
        if (!BuildEffectiveMaterialRecipe(primitive_comp, effective_recipe))
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: BuildEffectiveMaterialRecipe failed for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        if (!material_comp->bindings_dirty
         && !material_comp->resources_dirty
         && material_comp->data_index_row != uint32_t(-1)
         && material_comp->texture_layer_row != uint32_t(-1)
         && material_comp->material_instance_row != uint32_t(-1))
            return true;

        auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] Materialize failed: RenderDescriptorBindingSystem missing for %s",
                        GetPrimitiveOwnerName(primitive_comp));
            return false;
        }

        graph::mtl::MaterializationSpec spec{};
        uint32_t texture_layer_row = uint32_t(-1);
        uint32_t data_index_row = uint32_t(-1);
        if (!rdbs->ResolveMaterialRecipe(effective_recipe, spec, &texture_layer_row, &data_index_row))
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

        const uint32_t material_instance_row = ResolvePrimaryStructIndex(material_comp->program,
                                                                         spec,
                                                                         data_index_row);

        // data_index_row keeps the indirection-table row identity.
        // material_instance_row is the concrete struct_index that shaders use as mtl.mi[miID].
        material_comp->texture_layer_row = texture_layer_row;
        material_comp->data_index_row = data_index_row;
        material_comp->material_instance_row = material_instance_row;
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

            if (!primitiveComp->HasMaterialRecipe())
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

            auto item = std::make_unique<PrimitiveRenderItem>(entity_id, transform, primitiveComp, world);

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

