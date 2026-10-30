#include <hgl/graph/asset/SceneLoader.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/RenderableBuilder.h>
#include <hgl/graph/asset/GeometryPreset.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/ecs/components/CameraComponent.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/log/Log.h>
#include <hgl/type/StrChar.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <cstring>

DEFINE_LOGGER_MODULE(SceneLoader)
USE_MODULE_LOGGER(SceneLoader)

namespace hgl::graph
{
    namespace
    {
        bool CreateMaterial(const SceneMaterialSpec &spec,
                            GlobalSSBOBufferRegistry *registry,
                            SceneInstance::RuntimeMaterial *runtime)
        {
            if (!registry || !runtime)
                return false;

            runtime->name = spec.name;
            switch (spec.type)
            {
            case SceneMaterialType::PBRSurface:
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
            case SceneMaterialType::EmissiveSurface:
            {
                runtime->accessor = registry->GetAccessor<ssbo::EmissiveSurfaceRow>();
                ssbo::EmissiveSurfaceRow row{};
                row.color = spec.color;
                if (!runtime->accessor || !runtime->accessor.Write(row))
                    return false;
                break;
            }
            case SceneMaterialType::TransmissionSurface:
            {
                runtime->accessor = registry->GetAccessor<ssbo::TransmissionSurfaceRow>();
                ssbo::TransmissionSurfaceRow row{};
                row.trans_color = spec.trans_color.packUnorm4x8();
                if (!runtime->accessor || !runtime->accessor.Write(row))
                    return false;
                break;
            }
            default:
                return false;
            }

            for (int i = 0; i < spec.textures.GetCount(); ++i)
            {
                SceneTextureSpec *texture = runtime->textures.Create();
                if (!texture)
                    return false;
                texture->name = spec.textures[i]->name;
                texture->uri = spec.textures[i]->uri;
                texture->required = spec.textures[i]->required;
            }

            return true;
        }

        const SceneInstance::RuntimeMaterial *FindMaterial(
            const SceneInstance &instance,
            const AnsiString &name)
        {
            for (int i = 0; i < instance.GetMaterialCount(); ++i)
            {
                const SceneInstance::RuntimeMaterial *material = instance.GetMaterial(i);
                if (material && material->name == name)
                    return material;
            }
            return nullptr;
        }

        SceneInstance::RuntimeNode *FindNode(
            SceneInstance &instance,
            const AnsiString &name)
        {
            for (int i = 0; i < instance.GetNodeCount(); ++i)
            {
                SceneInstance::RuntimeNode *node = instance.GetNode(i);
                if (node && node->name == name)
                    return node;
            }
            return nullptr;
        }
    }

    SceneInstance::~SceneInstance()
    {
        Release();
    }

    void SceneInstance::Release()
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

    bool SceneLoader::Instantiate(const SceneSpec &spec,
                                  ecs::ECSContext *world,
                                  GeometryCreater *creater,
                                  SceneInstance &out_instance)
    {
        if (!world || !creater || out_instance.IsInstantiated()
            || spec.schema != 1 || spec.name.IsEmpty())
        {
            MLogError(SceneLoader, "Instantiate: invalid arguments, occupied instance, or unsupported schema");
            return false;
        }

        GraphicsContext *gc = world->GetGraphicsContext();
        if (!gc)
        {
            MLogError(SceneLoader, "Instantiate: ECS world has no GraphicsContext");
            return false;
        }

        GeometryManager *geometry_manager = gc->GetGeometryManager();
        GlobalSSBOBufferRegistry *ssbo_registry = gc->GetGlobalSSBOBufferRegistry();
        if (!geometry_manager || !ssbo_registry)
        {
            MLogError(SceneLoader, "Instantiate: geometry or SSBO registry is unavailable");
            return false;
        }

        out_instance.world = world;
        out_instance.geometry_manager = geometry_manager;

        for (int i = 0; i < spec.materials.GetCount(); ++i)
        {
            auto *runtime = out_instance.materials.Create();
            if (!runtime || !CreateMaterial(*spec.materials[i], ssbo_registry, runtime))
            {
                MLogError(SceneLoader, "Instantiate: failed to allocate scene material row");
                out_instance.Release();
                return false;
            }
        }

        for (int i = 0; i < spec.nodes.GetCount(); ++i)
        {
            const SceneNodeSpec &node_spec = *spec.nodes[i];
            if (node_spec.geometry.IsEmpty()
                || std::strncmp(node_spec.geometry.c_str(), "presets:", 8) != 0
                || node_spec.geometry.c_str()[8] == '\0')
            {
                MLogError(SceneLoader, "Instantiate: V1 only supports non-empty presets: geometry references");
                out_instance.Release();
                return false;
            }

            const mtl::MaterialRecipe *recipe =
                gc->FindOrLoadMaterialRecipe(node_spec.recipe.c_str());
            if (!recipe)
            {
                MLogError(SceneLoader, "Instantiate: material recipe could not be resolved");
                out_instance.Release();
                return false;
            }

            auto *runtime = out_instance.nodes.Create();
            if (!runtime)
            {
                MLogError(SceneLoader, "Instantiate: failed to allocate runtime node");
                out_instance.Release();
                return false;
            }

            runtime->name = node_spec.name;
            runtime->geometry = CreateGeometryPreset(creater, node_spec.geometry.c_str() + 8);
            if (!runtime->geometry)
            {
                MLogError(SceneLoader, "Instantiate: geometry preset could not be created");
                out_instance.Release();
                return false;
            }

            if (geometry_manager->Add(runtime->geometry) < 0)
            {
                MLogError(SceneLoader, "Instantiate: geometry manager rejected generated geometry");
                geometry_manager->Release(runtime->geometry);
                runtime->geometry = nullptr;
                out_instance.Release();
                return false;
            }

            runtime->recipe = *recipe;

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
                const SceneInstance::RuntimeMaterial *material =
                    FindMaterial(out_instance, node_spec.material);
                if (!material)
                {
                    MLogError(SceneLoader, "Instantiate: node references an unknown scene material");
                    out_instance.Release();
                    return false;
                }

                if (!material->accessor.GetGlobalSSBOBinding().IsValid())
                {
                    MLogError(SceneLoader, "Instantiate: scene material has no valid SSBO row");
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
                    const SceneTextureSpec &texture = *material->textures[texture_index];
                    TextureAsset *asset =
                        texture_manager
                            ? texture_manager->CreateTextureAsset(texture.uri, true)
                            : nullptr;
                    if (!asset
                        || !runtime->recipe.AddTexture(
                            texture.name.c_str(), asset, texture.required))
                    {
                        MLogError(SceneLoader, "Instantiate: scene texture could not be added to recipe");
                        out_instance.Release();
                        return false;
                    }
                }

                builder.WithMaterialDataResource(material->accessor.GetGlobalSSBOBinding());
            }

            runtime->primitive = PrimitiveAsset(runtime->geometry, &runtime->recipe);
            builder.WithPrimitiveAsset(&runtime->primitive);
            ecs::Entity *entity = builder.Build();
            if (!entity)
            {
                MLogError(SceneLoader, "Instantiate: ECS renderable construction failed");
                out_instance.Release();
                return false;
            }

            runtime->entity_id = entity->GetEntityID();
            world->SetEntityVisible(runtime->entity_id, node_spec.visible);
        }

        for (int i = 0; i < spec.nodes.GetCount(); ++i)
        {
            const SceneNodeSpec &node_spec = *spec.nodes[i];
            if (node_spec.parent.IsEmpty())
                continue;

            SceneInstance::RuntimeNode *child = FindNode(out_instance, node_spec.name);
            SceneInstance::RuntimeNode *parent = FindNode(out_instance, node_spec.parent);
            if (!child || !parent)
            {
                MLogError(SceneLoader, "Instantiate: parent relationship could not be resolved");
                out_instance.Release();
                return false;
            }

            ecs::TransformAccessor child_transform =
                world->GetTransformByEntity(child->entity_id);
            ecs::TransformAccessor parent_transform =
                world->GetTransformByEntity(parent->entity_id);
            if (!child_transform.IsValid() || !parent_transform.IsValid())
            {
                MLogError(SceneLoader, "Instantiate: parent transform is unavailable");
                out_instance.Release();
                return false;
            }
            child_transform.SetParent(parent_transform.GetID());
        }

        if (spec.camera.enabled)
        {
            if (!world->EnsureCameraSystem())
            {
                MLogError(SceneLoader, "Instantiate: camera system is unavailable");
                out_instance.Release();
                return false;
            }

            ecs::Entity *camera_entity =
                world->CreateEntity<ecs::Entity>("SceneCamera");
            if (!camera_entity)
            {
                MLogError(SceneLoader, "Instantiate: camera entity creation failed");
                out_instance.Release();
                return false;
            }
            out_instance.camera_entity_id = camera_entity->GetEntityID();

            ecs::CameraComponent *camera =
                world->GetOrCreateCamera(camera_entity, "SceneCamera");
            if (!camera)
            {
                MLogError(SceneLoader, "Instantiate: camera component creation failed");
                out_instance.Release();
                return false;
            }

            switch (spec.camera.mode)
            {
            case SceneCameraMode::ViewModel:
                camera->control_mode = ecs::CameraControlMode::ViewModel;
                break;
            case SceneCameraMode::Free:
                camera->control_mode = ecs::CameraControlMode::Free;
                break;
            case SceneCameraMode::LookAt:
                camera->control_mode = ecs::CameraControlMode::LookAt;
                break;
            case SceneCameraMode::FirstPerson:
                camera->control_mode = ecs::CameraControlMode::FirstPerson;
                break;
            default:
                MLogError(SceneLoader, "Instantiate: unsupported camera mode");
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
                MLogError(SceneLoader, "Instantiate: clear color requires a render target");
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
