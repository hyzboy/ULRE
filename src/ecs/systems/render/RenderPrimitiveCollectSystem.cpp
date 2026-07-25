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
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/log/Log.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    namespace
    {
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

        void SyncLegacyMaterialRuntime(ECSContext *world,
                                       const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                       const std::shared_ptr<MaterialComponent> &material_comp)
        {
            if (!world || !primitive_comp || !material_comp)
                return;

            if (!material_comp->program)
            {
                if (auto *program = primitive_comp->GetMaterialProgram())
                {
                    material_comp->program = program;
                    material_comp->program_dirty = false;
                }
            }

            auto *program = material_comp->program;
            auto *dbs = primitive_comp->GetDescriptorBindingSet();
            if (!program || !dbs)
                return;

            if (auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>())
            {
                const uint32_t mi_data_bytes = program->GetMIDataBytes();
                if (mi_data_bytes > 0)
                {
                    for (const auto &req : program->GetBindingContract().requirements)
                    {
                        if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                            continue;
                        rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes);
                    }
                }
            }

            for (const auto &req : program->GetBindingContract().requirements)
            {
                graph::DescriptorBindingSet::SSBOBinding binding{};
                if (!dbs->GetSSBOBinding(req.ssbo_type, binding))
                    continue;

                switch (req.semantic)
                {
                    case graph::mtl::DescriptorSemantic::MaterialInstance:
                        if (material_comp->material_instance_row == uint32_t(-1))
                            material_comp->material_instance_row = binding.slot_index;
                        if (material_comp->data_index_row == uint32_t(-1))
                            material_comp->data_index_row = binding.slot_index;
                        break;
                    case graph::mtl::DescriptorSemantic::MaterialTextureLayerTable:
                        if (material_comp->texture_layer_row == uint32_t(-1))
                            material_comp->texture_layer_row = binding.slot_index;
                        break;
                    case graph::mtl::DescriptorSemantic::MaterialDataIndexTable:
                        if (material_comp->data_index_row == uint32_t(-1))
                            material_comp->data_index_row = binding.slot_index;
                        break;
                    default:
                        break;
                }
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

        const auto *recipe = primitive_comp->GetMaterialRecipe();
        if (!recipe)
            return false;

        if (!material_comp->program_dirty && material_comp->program)
            return true;

        auto *graphics = world->GetGraphicsContext();
        if (!graphics)
            return false;

        auto *material_manager = graphics->GetMaterialManager();
        if (!material_manager)
            return false;

        graph::PrimitiveType primitive_type = graph::PrimitiveType::Triangles;
        if (auto *legacy_program = primitive_comp->GetMaterialProgram())
            primitive_type = legacy_program->GetPrimitiveType();

        graph::mtl::MaterialPreset preset{};
        bool resolved_by_model = false;
        switch (recipe->shading_model)
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
            if (!TryResolvePresetByHint(recipe->preset_hint, preset))
                return false;
        }
        else if (recipe->preset_hint != graph::mtl::InvalidMaterialPresetHint)
        {
            graph::mtl::MaterialPreset hinted{};
            if (TryResolvePresetByHint(recipe->preset_hint, hinted))
            {
                // For ambiguous models (e.g. Standard / Unlit), hint can refine concrete template.
                if (recipe->shading_model == graph::mtl::ShadingModel::Standard
                 || recipe->shading_model == graph::mtl::ShadingModel::Unlit)
                {
                    preset = hinted;
                }
            }
        }

        graph::MaterialProgram *resolved_program = nullptr;
        if (Is2DPreset(preset))
        {
            if (preset == graph::mtl::MaterialPreset::Text2D)
            {
                graph::mtl::Text2DMaterialCreateConfig cfg;
                cfg.prim = primitive_type;
                resolved_program = material_manager->AcquireMaterialProgram(preset, &cfg);
            }
            else
            {
                graph::mtl::Material2DCreateConfig cfg(primitive_type, graph::CoordinateSystem2D::NDC, graph::mtl::WithLocalToWorld::With);
                resolved_program = material_manager->AcquireMaterialProgram(preset, &cfg);
            }
        }
        else
        {
            if (preset == graph::mtl::MaterialPreset::SkyMinimal)
            {
                graph::mtl::SkyMinimalCreateConfig cfg(graph::mtl::WithCamera::With);
                cfg.prim = primitive_type;
                resolved_program = material_manager->AcquireMaterialProgram(preset, &cfg);
            }
            else
            {
                graph::mtl::Material3DCreateConfig cfg(primitive_type,
                                                       graph::mtl::WithCamera::With,
                                                       graph::mtl::WithLocalToWorld::With,
                                                       graph::mtl::WithSky::With);
                resolved_program = material_manager->AcquireMaterialProgram(preset, &cfg);
            }
        }

        if (!resolved_program)
            return false;

        if (auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            const uint32_t mi_data_bytes = resolved_program->GetMIDataBytes();
            if (mi_data_bytes > 0)
            {
                for (const auto &req : resolved_program->GetBindingContract().requirements)
                {
                    if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                        continue;

                    rdbs->RegisterMaterialStructLayout(req.ssbo_type, req.ssbo_id, mi_data_bytes);
                }
            }
        }

        material_comp->program = resolved_program;
        material_comp->program_dirty = false;
        return true;
    }

    bool RenderPrimitiveCollectSystem::MaterializeRecipeRowsForPrimitive(const std::shared_ptr<PrimitiveComponent> &primitive_comp,
                                                                         const std::shared_ptr<MaterialComponent> &material_comp)
    {
        if (!world || !primitive_comp || !material_comp)
            return false;

        const auto *recipe = primitive_comp->GetMaterialRecipe();
        if (!recipe)
            return false;

        if (!material_comp->bindings_dirty
         && !material_comp->resources_dirty
         && material_comp->data_index_row != uint32_t(-1)
         && material_comp->texture_layer_row != uint32_t(-1))
            return true;

        auto rdbs = world->GetSystem<RenderDescriptorBindingSystem>();
        if (!rdbs)
            return false;

        graph::mtl::MaterializationSpec spec{};
        uint32_t texture_layer_row = uint32_t(-1);
        uint32_t data_index_row = uint32_t(-1);
        if (!rdbs->ResolveMaterialRecipe(*recipe, spec, &texture_layer_row, &data_index_row))
            return false;

        // Bridge stage: DataIndexRow is the per-instance lookup row consumed by
        // ResolveDataIndexID(gl_InstanceIndex). Keep material_instance_row aligned.
        material_comp->texture_layer_row = texture_layer_row;
        material_comp->data_index_row = data_index_row;
        material_comp->material_instance_row = data_index_row;
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
                SyncLegacyMaterialRuntime(world, primitiveComp, material_comp);

            if (material_comp && primitiveComp->HasMaterialRecipe())
            {
                ResolveMaterialProgramForPrimitive(primitiveComp, material_comp);
                MaterializeRecipeRowsForPrimitive(primitiveComp, material_comp);
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
