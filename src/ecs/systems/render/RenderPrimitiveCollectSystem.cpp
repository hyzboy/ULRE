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
#include<hgl/graph/DescriptorBindingSet.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
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

        bool TryResolvePresetByHint(const uint32_t hint, graph::mtl::MaterialPreset &out_preset)
        {
            if (hint == graph::mtl::InvalidMaterialPresetHint)
                return false;

            if (hint >= static_cast<uint32_t>(graph::mtl::MaterialPreset::RANGE_SIZE))
                return false;

            out_preset = static_cast<graph::mtl::MaterialPreset>(hint);
            return true;
        }

        bool Is2DPreset(const graph::mtl::MaterialPreset preset)
        {
            switch (preset)
            {
                case graph::mtl::MaterialPreset::VertexColor2D:
                case graph::mtl::MaterialPreset::PureColor2D:
                case graph::mtl::MaterialPreset::PureTexture2D:
                case graph::mtl::MaterialPreset::RectTexture2D:
                case graph::mtl::MaterialPreset::RectTexture2DArray:
                case graph::mtl::MaterialPreset::Text2D:
                    return true;
                default:
                    return false;
            }
        }

        bool Is2DInlinePipeline(const graph::InlinePipeline pipeline)
        {
            switch (pipeline)
            {
                case graph::InlinePipeline::Solid2D:
                case graph::InlinePipeline::Alpha2D:
                    return true;
                default:
                    return false;
            }
        }

        bool IsLikely2DPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                 const graph::GeometryVertexFormat *geometry_vertex_format)
        {
            if (primitive_comp && primitive_comp->HasPendingPipelinePreset())
                return Is2DInlinePipeline(primitive_comp->GetPendingPipelinePreset());

            const auto *position = geometry_vertex_format ? geometry_vertex_format->Find(graph::VertexSemantic::Position) : nullptr;
            return position && position->vec_size == 2;
        }

        bool TryResolveUnlitPresetFromGeometry(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                               const graph::GeometryVertexFormat *geometry_vertex_format,
                                               graph::mtl::MaterialPreset &out_preset)
        {
            if (!geometry_vertex_format)
                return false;

            const bool is_2d = IsLikely2DPrimitive(primitive_comp, geometry_vertex_format);

            if (geometry_vertex_format->Find(graph::VertexSemantic::Luminance))
            {
                if (is_2d)
                    return false;

                out_preset = graph::mtl::MaterialPreset::VertexLuminance3D;
                return true;
            }

            if (geometry_vertex_format->Find(graph::VertexSemantic::Color))
            {
                out_preset = is_2d
                           ? graph::mtl::MaterialPreset::VertexColor2D
                           : graph::mtl::MaterialPreset::VertexColor3D;
                return true;
            }

            if (geometry_vertex_format->Find(graph::VertexSemantic::Normal))
            {
                out_preset = graph::mtl::MaterialPreset::Gizmo3D;
                return true;
            }

            return false;
        }

        bool HasResourceSemantic(const graph::MaterialProgram *material,
                                 const graph::mtl::DescriptorSemantic semantic)
        {
            if (!material)
                return false;

            for (const auto &req : material->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic == semantic)
                    return true;
            }

            return false;
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

        uint32_t ResolvePrimaryStructIndex(const graph::MaterialProgram *material,
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
                        if (ref.slot == req.data_slot && ref.ssbo_type == req.ssbo_type)
                            return ref.struct_index;
                    }
                }
            }

            if (!spec.struct_refs.empty())
                return spec.struct_refs.front().struct_index;

            return fallback_row;
        }

        bool ReferenceProgramMatchesPreset(const graph::MaterialProgram *material,
                                           const graph::mtl::MaterialPreset preset)
        {
            if (!material)
                return false;

            const char *material_name = material->GetName().c_str();
            const char *preset_name = graph::mtl::GetMaterialPresetName(preset);
            if (!material_name || !preset_name)
                return false;

            const size_t preset_name_len = std::strlen(preset_name);
            if (preset_name_len == 0)
                return false;

            return std::strncmp(material_name, preset_name, preset_name_len) == 0;
        }

        std::string BuildTextureResourceId(graph::Texture *texture)
        {
            if (!texture)
                return {};

            return "texid:" + std::to_string(texture->GetID());
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
                                       graph::mtl::DataSlot slot,
                                       graph::mtl::SSBOType ssbo_type,
                                       const uint32_t ssbo_id,
                                       const uint32_t struct_index,
                                       const bool use_struct_index,
                                       const bool shared_across_instances)
        {
            for (auto &binding : recipe.structs)
            {
                if (binding.slot != slot)
                    continue;

                binding.ssbo_type = ssbo_type;
                binding.ssbo_id = ssbo_id;
                binding.struct_index = struct_index;
                binding.use_struct_index = use_struct_index;
                binding.shared_across_instances = shared_across_instances;
                return;
            }

            graph::mtl::RecipeStructBinding binding{};
            binding.slot = slot;
            binding.ssbo_type = ssbo_type;
            binding.ssbo_id = ssbo_id;
            binding.struct_index = struct_index;
            binding.use_struct_index = use_struct_index;
            binding.shared_across_instances = shared_across_instances;
            recipe.structs.emplace_back(std::move(binding));
        }

        void NormalizeRecipeWithBaseMaterialInfo(graph::mtl::MaterialRecipe &recipe)
        {
            graph::mtl::BaseMaterialInfo bmi{};
            bool has_bmi = false;

            if (!recipe.base_material_info_name.empty())
            {
                has_bmi = graph::mtl::TryGetBaseMaterialInfoByName(recipe.base_material_info_name, bmi);
            }
            else if (recipe.preset_hint != graph::mtl::InvalidMaterialPresetHint
                  && recipe.preset_hint < static_cast<uint32_t>(graph::mtl::MaterialPreset::RANGE_SIZE))
            {
                const graph::mtl::MaterialPreset preset = static_cast<graph::mtl::MaterialPreset>(recipe.preset_hint);
                has_bmi = graph::mtl::TryGetBaseMaterialInfoByPreset(preset, bmi);
            }

            if (has_bmi)
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

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::DataSlot::RANGE_SIZE); ++i)
            {
                const auto slot = static_cast<graph::mtl::DataSlot>(i);
                const auto *resource = primitive_comp->GetMaterialStructResource(slot);
                if (!resource)
                    continue;

                UpsertRecipeStructBinding(out_recipe,
                                         slot,
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
                                             graph::MaterialProgram *material_program)
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

            for (size_t i = 0; i < static_cast<size_t>(graph::mtl::DataSlot::RANGE_SIZE); ++i)
            {
                const auto slot = static_cast<graph::mtl::DataSlot>(i);
                const auto *resource = primitive_comp->GetMaterialStructResource(slot);
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

                if (!primitive_comp->GetMaterialStructResource(req.data_slot) && named_struct_resource)
                {
                    primitive_comp->SetMaterialStructResource(req.data_slot,
                                                              req.ssbo_type,
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

        void ResetMaterialRuntimeForLegacy(const std::shared_ptr<MaterialComponent> &material_comp)
        {
            if (!material_comp)
                return;

            material_comp->program = nullptr;
            material_comp->program_dirty = true;
            material_comp->material_instance_row = uint32_t(-1);
            material_comp->texture_layer_row = uint32_t(-1);
            material_comp->data_index_row = uint32_t(-1);
            material_comp->bindings_dirty = true;
            material_comp->resources_dirty = true;
            material_comp->valid = false;
            material_comp->recipe_hash = 0;
        }

        void SyncLegacyMaterialRuntime(ECSContext *world,
                                       const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                       const std::shared_ptr<MaterialComponent> &material_comp)
        {
            if (!world || !primitive_comp || !material_comp)
                return;

            auto *current_program = primitive_comp->GetMaterialProgram();
            if (!current_program)
            {
                ResetMaterialRuntimeForLegacy(material_comp);
                return;
            }

            if (material_comp->program != current_program)
            {
                material_comp->program = current_program;
                material_comp->program_dirty = false;
                material_comp->material_instance_row = uint32_t(-1);
                material_comp->texture_layer_row = uint32_t(-1);
                material_comp->data_index_row = uint32_t(-1);
                material_comp->bindings_dirty = true;
                material_comp->resources_dirty = true;
                material_comp->valid = false;
            }

            auto *program = current_program;
            auto *dbs = primitive_comp->GetInternalDescriptorBindingSet();
            if (!program)
            {
                ResetMaterialRuntimeForLegacy(material_comp);
                return;
            }

            if (!dbs)
            {
                material_comp->material_instance_row = uint32_t(-1);
                material_comp->texture_layer_row = uint32_t(-1);
                material_comp->data_index_row = uint32_t(-1);
                material_comp->bindings_dirty = true;
                material_comp->resources_dirty = true;
                material_comp->valid = false;
                return;
            }

            if (auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>())
            {
                const uint32_t mi_data_bytes = program->GetMIDataBytes();
                if (mi_data_bytes > 0)
                {
                    for (const auto &req : program->GetMaterialResourceLayout().requirements)
                    {
                        if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                            continue;
                        rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes);
                    }
                }
            }

            uint32_t material_instance_row = uint32_t(-1);
            uint32_t texture_layer_row = uint32_t(-1);
            uint32_t data_index_row = uint32_t(-1);

            for (const auto &req : program->GetMaterialResourceLayout().requirements)
            {
                graph::DescriptorBindingSet::SSBOBinding binding{};
                if (!dbs->GetSSBOBinding(req.ssbo_type, binding))
                    continue;

                switch (req.semantic)
                {
                    case graph::mtl::DescriptorSemantic::MaterialInstance:
                        material_instance_row = binding.slot_index;
                        if (data_index_row == uint32_t(-1))
                            data_index_row = binding.slot_index;
                        break;
                    case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
                        texture_layer_row = binding.slot_index;
                        break;
                    case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
                        data_index_row = binding.slot_index;
                        break;
                    default:
                        break;
                }
            }

            material_comp->material_instance_row = material_instance_row;
            material_comp->texture_layer_row = texture_layer_row;
            material_comp->data_index_row = data_index_row;
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
        graph::MaterialProgram *reference_program = nullptr;
        const graph::GeometryVertexFormat *geometry_vertex_format = nullptr;
        if (const auto *asset = primitive_comp->GetPrimitiveAsset())
        {
            if (auto *asset_geometry = asset->GetGeometry())
                geometry_vertex_format = &asset_geometry->GetGeometryVertexFormat();
            primitive_type = asset->GetPrimitiveType();
        }

        graph::mtl::MaterialPreset preset{};
        bool resolved_by_model = false;
        switch (effective_recipe.shading_model)
        {
            case graph::mtl::ShadingModel::Text:
                preset = graph::mtl::MaterialPreset::Text2D;
                resolved_by_model = true;
                break;
            case graph::mtl::ShadingModel::Sky:
                preset = graph::mtl::MaterialPreset::SkyMinimal;
                resolved_by_model = true;
                break;
            case graph::mtl::ShadingModel::Standard:
                preset = graph::mtl::MaterialPreset::Standard;
                resolved_by_model = true;
                break;
            case graph::mtl::ShadingModel::Unlit:
                if (!TryResolveUnlitPresetFromGeometry(primitive_comp, geometry_vertex_format, preset))
                    preset = graph::mtl::MaterialPreset::Gizmo3D;
                resolved_by_model = true;
                break;
            case graph::mtl::ShadingModel::Legacy:
            case graph::mtl::ShadingModel::Custom:
            case graph::mtl::ShadingModel::Unknown:
            default:
                break;
        }

        // Fallback bridge: use preset_hint only when shading-model policy is insufficient.
        if (!resolved_by_model)
        {
            if (!TryResolvePresetByHint(effective_recipe.preset_hint, preset))
            {
                GLogWarning("[RenderPrimitiveCollectSystem] ResolveMaterialProgram failed: no preset for %s recipe=%s",
                            GetPrimitiveOwnerName(primitive_comp),
                            effective_recipe.recipe_name.c_str());
                return false;
            }
        }
        else if (effective_recipe.preset_hint != graph::mtl::InvalidMaterialPresetHint)
        {
            graph::mtl::MaterialPreset hinted{};
            if (TryResolvePresetByHint(effective_recipe.preset_hint, hinted))
            {
                // For ambiguous models (e.g. Standard / Unlit), hint can refine concrete template.
                if (effective_recipe.shading_model == graph::mtl::ShadingModel::Standard
                 || effective_recipe.shading_model == graph::mtl::ShadingModel::Unlit)
                {
                    preset = hinted;
                }
            }
        }

        graph::MaterialProgram *resolved_program = nullptr;
        if (ReferenceProgramMatchesPreset(reference_program, preset))
            resolved_program = reference_program;

        if (!resolved_program && Is2DPreset(preset))
        {
            if (preset == graph::mtl::MaterialPreset::Text2D)
            {
                graph::mtl::Text2DMaterialCreateConfig cfg;
                cfg.prim = primitive_type;
                resolved_program = material_manager->AcquireMaterialProgram(preset, &cfg);
            }
            else
            {
                const graph::mtl::WithLocalToWorld with_l2w =
                    effective_recipe.local_to_world_2d
                  ? graph::mtl::WithLocalToWorld::With
                  : graph::mtl::WithLocalToWorld::Without;

                graph::mtl::Material2DCreateConfig cfg(primitive_type,
                                                       effective_recipe.coordinate_system_2d,
                                                       with_l2w);
                resolved_program = geometry_vertex_format
                                 ? material_manager->AcquireMaterialProgram(preset, &cfg, *geometry_vertex_format)
                                 : material_manager->AcquireMaterialProgram(preset, &cfg);
            }
        }
        else if (!resolved_program)
        {
            if (preset == graph::mtl::MaterialPreset::SkyMinimal)
            {
                graph::mtl::SkyMinimalCreateConfig cfg(graph::mtl::WithCamera::With);
                cfg.prim = primitive_type;
                resolved_program = material_manager->AcquireMaterialProgram(preset, &cfg);
            }
            else
            {
                const graph::mtl::WithCamera with_camera =
                    HasResourceSemantic(reference_program, graph::mtl::DescriptorSemantic::CameraInfo)
                    ? graph::mtl::WithCamera::With
                    : graph::mtl::WithCamera::Without;
                const graph::mtl::WithLocalToWorld with_l2w =
                    (reference_program && reference_program->hasLocalToWorld())
                    ? graph::mtl::WithLocalToWorld::With
                    : graph::mtl::WithLocalToWorld::Without;
                const graph::mtl::WithSky with_sky =
                    (HasResourceSemantic(reference_program, graph::mtl::DescriptorSemantic::SkyInfo)
                  || HasResourceSemantic(reference_program, graph::mtl::DescriptorSemantic::SkyCubemapSampler))
                    ? graph::mtl::WithSky::With
                    : graph::mtl::WithSky::Without;

                graph::mtl::Material3DCreateConfig cfg(primitive_type,
                                                       with_camera,
                                                       with_l2w,
                                                       with_sky);
                resolved_program = geometry_vertex_format
                                 ? material_manager->AcquireMaterialProgram(preset, &cfg, *geometry_vertex_format)
                                 : material_manager->AcquireMaterialProgram(preset, &cfg);
            }
        }

        if (!resolved_program)
        {
            GLogWarning("[RenderPrimitiveCollectSystem] AcquireMaterialProgram failed for %s recipe=%s preset=%u",
                        GetPrimitiveOwnerName(primitive_comp),
                        effective_recipe.recipe_name.c_str(),
                        static_cast<uint32_t>(preset));
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

            auto material_comp = entity->GetComponent<MaterialComponent>();
            if (!material_comp
             && (primitiveComp->HasMaterialRecipe()
              || primitiveComp->GetMaterialProgram()))
                material_comp = entity->AddComponent<MaterialComponent>();

            if (material_comp && !primitiveComp->HasMaterialRecipe())
            {
                if (material_comp->recipe_hash != 0)
                    ResetMaterialRuntimeForLegacy(material_comp);
                SyncLegacyMaterialRuntime(world, primitiveComp, material_comp);
            }

            if (material_comp && primitiveComp->HasMaterialRecipe())
            {
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
