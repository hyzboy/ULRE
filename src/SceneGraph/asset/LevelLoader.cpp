#include <hgl/graph/asset/LevelLoader.h>
#include <hgl/graph/asset/GeometryAssetRegistry.h>
#include <hgl/graph/asset/PrimitiveAssetRegistry.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/pipeline/RenderableBuilder.h>
#include <hgl/graph/asset/GeometryPreset.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/filesystem/Path.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/log/Log.h>
#include <hgl/type/StrChar.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <cstring>

DEFINE_LOGGER_MODULE(LevelLoader)
USE_MODULE_LOGGER(LevelLoader)

namespace hgl::graph
{
    namespace
    {
        OSString ResolveLevelPath(const OSString &level_filename,
                                  const OSString &reference)
        {
            if (reference.IsEmpty())
                return reference;

            const os_char *path = reference.c_str();
            if (path[0] == os_char('/')
                || path[0] == os_char('\\')
                || (reference.Length() >= 2 && path[1] == os_char(':')))
                return hgl::filesystem::NormalizeFilename(reference);

            if (level_filename.IsEmpty())
                return hgl::filesystem::NormalizeFilename(reference);

            hgl::filesystem::Path resolved =
                hgl::filesystem::Path(level_filename).GetParent();
            resolved /= reference;
            resolved.Normalize();
            return resolved.ToOSString();
        }

        bool HasPrefix(const AnsiString &value, const char *prefix)
        {
            const char *text = value.c_str();
            return text && std::strncmp(text, prefix, std::strlen(prefix)) == 0;
        }

        bool CreateMaterial(const LevelMaterialSpec &spec,
                            GlobalSSBOBufferRegistry *registry,
                            LevelInstance::RuntimeMaterial *runtime)
        {
            if (!registry || !runtime)
                return false;

            runtime->name = spec.name;
            switch (spec.type)
            {
            case LevelMaterialType::PBRSurface:
            {
                runtime->accessor = registry->GetAccessor<ssbo::PBRSurfaceRow>();
                ssbo::PBRSurfaceRow row{};
                row.base_color = spec.base_color;
                row.metallic = spec.metallic;
                row.roughness = spec.roughness;
                row.normal_scale = spec.normal_scale;
                row.fresnel = spec.fresnel;
                if (!runtime->accessor || !runtime->accessor.Write(row))
                    return false;
                break;
            }
            case LevelMaterialType::EmissiveSurface:
            {
                runtime->accessor = registry->GetAccessor<ssbo::EmissiveSurfaceRow>();
                ssbo::EmissiveSurfaceRow row{};
                row.color = spec.color;
                if (!runtime->accessor || !runtime->accessor.Write(row))
                    return false;
                break;
            }
            case LevelMaterialType::TransmissionSurface:
            {
                runtime->accessor = registry->GetAccessor<ssbo::TransmissionSurfaceRow>();
                ssbo::TransmissionSurfaceRow row{};
                row.trans_color = spec.trans_color.packUnorm4x8();
                if (!runtime->accessor || !runtime->accessor.Write(row))
                    return false;
                break;
            }
            case LevelMaterialType::PBRAdvancedSurface:
            {
                runtime->accessor = registry->GetAccessor<ssbo::PBRAdvancedSurfaceRow>();
                ssbo::PBRAdvancedSurfaceRow row{};
                row.base_color = spec.base_color;
                row.metallic = spec.metallic;
                row.roughness = spec.roughness;
                row.normal_scale = spec.normal_scale;
                row.fresnel = spec.fresnel;
                row.clearcoat = spec.clearcoat;
                row.clearcoat_roughness = spec.clearcoat_roughness;
                row.sheen_roughness = spec.sheen_roughness;
                row.anisotropy = spec.anisotropy;
                row.sheen_color = spec.sheen_color.packUnorm4x8();
                row.subsurface_power = spec.subsurface_power;
                row.thickness = spec.thickness;
                if (!runtime->accessor || !runtime->accessor.Write(row))
                    return false;
                break;
            }
            default:
                return false;
            }

            for (int i = 0; i < spec.textures.GetCount(); ++i)
            {
                LevelTextureSpec *texture = runtime->textures.Create();
                if (!texture)
                    return false;
                texture->name = spec.textures[i]->name;
                texture->uri = spec.textures[i]->uri;
                texture->required = spec.textures[i]->required;
            }

            return true;
        }

        const LevelInstance::RuntimeMaterial *FindMaterial(
            const LevelInstance &instance,
            const AnsiString &name)
        {
            for (int i = 0; i < instance.GetMaterialCount(); ++i)
            {
                const LevelInstance::RuntimeMaterial *material = instance.GetMaterial(i);
                if (material && material->name == name)
                    return material;
            }
            return nullptr;
        }

        LevelInstance::RuntimeNode *FindNode(
            LevelInstance &instance,
            const AnsiString &name)
        {
            for (int i = 0; i < instance.GetNodeCount(); ++i)
            {
                LevelInstance::RuntimeNode *node = instance.GetNode(i);
                if (node && node->name == name)
                    return node;
            }
            return nullptr;
        }
    }

    LevelInstance::~LevelInstance()
    {
        Release();
    }

    void LevelInstance::Release()
    {
        if (world)
            for (int i = 0; i < nodes.GetCount(); ++i)
                if (nodes[i]->entity_id.IsValid())
                    world->DestroyEntity(nodes[i]->entity_id);

        if (geometry_manager)
            for (int i = 0; i < nodes.GetCount(); ++i)
                if (nodes[i]->geometry)
                    geometry_manager->Release(nodes[i]->geometry);

        nodes.Clear();
        materials.Clear();
        if (world && camera_entity_id.IsValid())
            world->DestroyEntity(camera_entity_id);
        camera_entity_id = ecs::EntityID::Invalid();
        if (clear_color_changed && world)
            if (auto *render_target = world->GetRenderTarget())
                render_target->SetClearColor(previous_clear_color);
        clear_color_changed = false;
        world = nullptr;
        geometry_manager = nullptr;
    }

    bool LevelLoader::Instantiate(const LevelSpec &spec,
                                  ecs::ECSContext *world,
                                  GeometryCreater *creater,
                                  LevelInstance &out_instance,
                                  const OSString &level_filename)
    {
        if (!world || !creater || out_instance.IsInstantiated()
            || spec.schema != 1 || spec.name.IsEmpty())
        {
            MLogError(LevelLoader, "Instantiate: invalid arguments, occupied instance, or unsupported schema");
            return false;
        }

        GraphicsContext *gc = world->GetGraphicsContext();
        if (!gc)
        {
            MLogError(LevelLoader, "Instantiate: ECS world has no GraphicsContext");
            return false;
        }

        GeometryManager *geometry_manager = gc->GetGeometryManager();
        GlobalSSBOBufferRegistry *ssbo_registry = gc->GetGlobalSSBOBufferRegistry();
        if (!geometry_manager || !ssbo_registry)
        {
            MLogError(LevelLoader, "Instantiate: geometry or SSBO registry is unavailable");
            return false;
        }

        out_instance.world = world;
        out_instance.geometry_manager = geometry_manager;

        for (int i = 0; i < spec.materials.GetCount(); ++i)
        {
            auto *runtime = out_instance.materials.Create();
            if (!runtime || !CreateMaterial(*spec.materials[i], ssbo_registry, runtime))
            {
                MLogError(LevelLoader, "Instantiate: failed to allocate scene material row");
                out_instance.Release();
                return false;
            }
        }

        for (int i = 0; i < spec.nodes.GetCount(); ++i)
        {
            const LevelNodeSpec &node_spec = *spec.nodes[i];
            const bool uses_preset = HasPrefix(node_spec.geometry, "presets:");
            const bool uses_geometry_asset = HasPrefix(node_spec.geometry, "geo:");
            const bool uses_primitive_asset = HasPrefix(node_spec.geometry, "primitive:");
            const mtl::MaterialRecipe *recipe = nullptr;
            if (!uses_primitive_asset)
                recipe = gc->FindOrLoadMaterialRecipe(node_spec.recipe.c_str());
            if (!uses_primitive_asset && !recipe)
            {
                MLogError(LevelLoader, "Instantiate: material recipe could not be resolved");
                out_instance.Release();
                return false;
            }

            auto *runtime = out_instance.nodes.Create();
            if (!runtime)
            {
                MLogError(LevelLoader, "Instantiate: failed to allocate runtime node");
                out_instance.Release();
                return false;
            }

            runtime->name = node_spec.name;
            if (recipe)
                runtime->recipe = *recipe;

            if (uses_preset)
            {
                runtime->geometry =
                    CreateGeometryPreset(creater, node_spec.geometry.c_str() + 8);
                if (!runtime->geometry)
                {
                    MLogError(LevelLoader, "Instantiate: geometry preset could not be created");
                    out_instance.Release();
                    return false;
                }

                if (geometry_manager->Add(runtime->geometry) < 0)
                {
                    MLogError(LevelLoader, "Instantiate: geometry manager rejected generated geometry");
                    geometry_manager->Release(runtime->geometry);
                    runtime->geometry = nullptr;
                    out_instance.Release();
                    return false;
                }
            }
            else if (uses_geometry_asset)
            {
                GeometryAssetRegistry *registry = gc->GetGeometryAssetRegistry();
                const OSString asset_path = ResolveLevelPath(
                    level_filename,
                    ToOSString(AnsiString(node_spec.geometry.c_str() + 4)));
                runtime->geometry_asset =
                    registry ? registry->GetOrLoad(asset_path) : nullptr;
                if (!runtime->geometry_asset)
                {
                    MLogError(LevelLoader, "Instantiate: geometry asset could not be loaded");
                    out_instance.Release();
                    return false;
                }
            }

            ecs::RenderableBuilder builder(world, node_spec.name.c_str());
            const ecs::Mobility mobility =
                hgl::stricmp(node_spec.mobility.c_str(), "static") == 0
                    ? ecs::Mobility::Static
                    : ecs::Mobility::Movable;

            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if (node_spec.transform.rotation_angle_deg != 0.0f)
            {
                const math::Vector3f &axis = node_spec.transform.rotation_axis;
                const glm::vec3 normalized_axis = glm::normalize(glm::vec3(axis.x, axis.y, axis.z));
                rotation = glm::angleAxis(glm::radians(node_spec.transform.rotation_angle_deg),
                                          normalized_axis);
            }

            builder.WithTransform(
                glm::vec3(node_spec.transform.position.x,
                          node_spec.transform.position.y,
                          node_spec.transform.position.z),
                rotation,
                glm::vec3(node_spec.transform.scale.x,
                          node_spec.transform.scale.y,
                          node_spec.transform.scale.z),
                mobility);

            if (!node_spec.material.IsEmpty())
            {
                const LevelInstance::RuntimeMaterial *material =
                    FindMaterial(out_instance, node_spec.material);
                if (!material)
                {
                    MLogError(LevelLoader, "Instantiate: node references an unknown scene material");
                    out_instance.Release();
                    return false;
                }

                if (!material->accessor.GetGlobalSSBOBinding().IsValid())
                {
                    MLogError(LevelLoader, "Instantiate: scene material has no valid SSBO row");
                    out_instance.Release();
                    return false;
                }

                runtime->recipe.material_ssbo_binding =
                    material->accessor.GetGlobalSSBOBinding();

                TextureManager *texture_manager = gc->GetTextureManager();
                for (int texture_index = 0;
                     texture_index < material->textures.GetCount();
                     ++texture_index)
                {
                    const LevelTextureSpec &texture = *material->textures[texture_index];
                    OSString texture_uri = texture.uri;
                    if (texture_uri.StartsWithIgnoreCase(OS_TEXT("tex:")))
                        texture_uri = OSString(texture_uri.c_str() + 4);
                    texture_uri = ResolveLevelPath(level_filename, texture_uri);
                    TextureAsset *asset =
                        texture_manager
                            ? texture_manager->CreateTextureAsset(texture_uri, true)
                            : nullptr;
                    if (!asset
                        || !runtime->recipe.AddTexture(
                            texture.name.c_str(), asset, texture.required))
                    {
                        MLogError(LevelLoader, "Instantiate: scene texture could not be added to recipe");
                        out_instance.Release();
                        return false;
                    }
                }

                builder.WithMaterialDataResource(material->accessor.GetGlobalSSBOBinding());
            }

            if (uses_primitive_asset)
            {
                PrimitiveAssetRegistry *registry = gc->GetPrimitiveAssetRegistry();
                const OSString asset_path = ResolveLevelPath(
                    level_filename,
                    ToOSString(AnsiString(node_spec.geometry.c_str() + 10)));
                const PrimitiveAsset *primitive =
                    registry ? registry->GetOrLoad(asset_path) : nullptr;
                if (!primitive)
                {
                    MLogError(LevelLoader, "Instantiate: primitive asset could not be instantiated");
                    out_instance.Release();
                    return false;
                }

                builder.WithPrimitiveAsset(primitive);
                ecs::Entity *entity = builder.Build();
                if (!entity)
                {
                    MLogError(LevelLoader, "Instantiate: ECS primitive asset construction failed");
                    out_instance.Release();
                    return false;
                }

                runtime->entity_id = entity->GetEntityID();
                world->SetEntityVisible(runtime->entity_id, node_spec.visible);
            }
            else
            {
                runtime->variant.material_recipe = &runtime->recipe;
                if (runtime->geometry_asset)
                    runtime->primitive = PrimitiveAsset(
                        runtime->geometry_asset->GetGeometry(),
                        runtime->geometry_asset,
                        &runtime->variant,
                        1);
                else
                    runtime->primitive = PrimitiveAsset(
                        runtime->geometry, &runtime->recipe);
                builder.WithPrimitiveAsset(&runtime->primitive);

                ecs::Entity *entity = builder.Build();
                if (!entity)
                {
                    MLogError(LevelLoader, "Instantiate: ECS renderable construction failed");
                    out_instance.Release();
                    return false;
                }

                runtime->entity_id = entity->GetEntityID();
                world->SetEntityVisible(runtime->entity_id, node_spec.visible);
                continue;
            }
        }

        for (int i = 0; i < spec.nodes.GetCount(); ++i)
        {
            const LevelNodeSpec &node_spec = *spec.nodes[i];
            if (node_spec.parent.IsEmpty())
                continue;

            LevelInstance::RuntimeNode *child = FindNode(out_instance, node_spec.name);
            LevelInstance::RuntimeNode *parent = FindNode(out_instance, node_spec.parent);
            if (!child || !parent)
            {
                MLogError(LevelLoader, "Instantiate: parent relationship could not be resolved");
                out_instance.Release();
                return false;
            }

            ecs::TransformAccessor child_transform =
                world->GetTransformByEntity(child->entity_id);
            ecs::TransformAccessor parent_transform =
                world->GetTransformByEntity(parent->entity_id);
            if (!child_transform.IsValid() || !parent_transform.IsValid())
            {
                MLogError(LevelLoader, "Instantiate: parent transform is unavailable");
                out_instance.Release();
                return false;
            }
            child_transform.SetParent(parent_transform.GetID());
        }

        if (spec.camera.enabled)
        {
            if (!world->EnsureCameraSystem())
            {
                MLogError(LevelLoader, "Instantiate: camera system is unavailable");
                out_instance.Release();
                return false;
            }

            ecs::Entity *camera_entity =
                world->CreateEntity<ecs::Entity>("LevelCamera");
            if (!camera_entity)
            {
                MLogError(LevelLoader, "Instantiate: camera entity creation failed");
                out_instance.Release();
                return false;
            }
            out_instance.camera_entity_id = camera_entity->GetEntityID();

            ecs::CameraComponent *camera =
                world->GetOrCreateCamera(camera_entity, "LevelCamera");
            if (!camera)
            {
                MLogError(LevelLoader, "Instantiate: camera component creation failed");
                out_instance.Release();
                return false;
            }

            switch (spec.camera.mode)
            {
            case LevelCameraMode::ViewModel:
                camera->control_mode = ecs::CameraControlMode::ViewModel;
                break;
            case LevelCameraMode::Free:
                camera->control_mode = ecs::CameraControlMode::Free;
                break;
            case LevelCameraMode::LookAt:
                camera->control_mode = ecs::CameraControlMode::LookAt;
                break;
            case LevelCameraMode::FirstPerson:
                camera->control_mode = ecs::CameraControlMode::FirstPerson;
                break;
            default:
                MLogError(LevelLoader, "Instantiate: unsupported camera mode");
                out_instance.Release();
                return false;
            }

            camera->position = spec.camera.position;
            camera->target = spec.camera.target;
            camera->distance = spec.camera.distance;
            camera->yaw = spec.camera.yaw;
            camera->pitch = spec.camera.pitch;
            camera->fov = spec.camera.fov_deg;
            camera->near_plane = spec.camera.near_plane;
            camera->far_plane = spec.camera.far_plane;
            camera->is_main_camera = spec.camera.main;
            camera->matrix_dirty = true;
        }

        if (spec.clear_color.enabled)
        {
            auto *render_target = world->GetRenderTarget();
            if (!render_target)
            {
                MLogError(LevelLoader, "Instantiate: clear color requires a render target");
                out_instance.Release();
                return false;
            }
            out_instance.previous_clear_color = render_target->GetClearColor();
            render_target->SetClearColor(spec.clear_color.value);
            out_instance.clear_color_changed = true;
        }

        return true;
    }
}
