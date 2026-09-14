#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateSkyCubeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }
}

/**
 * Cubemap 天空球示例：
 *
 * 使用 sky_34_cubemap_2k.TexCube(Cubemap 纹理)作为天空球材质，
 * 在屏幕中央渲染一个由 CubeMap 纹理贴图的球体。
 *
 * 材质："SkyCube"(ShaderLibrary/material/sky_cube.material.toml)
 *       以 normalize(world_pos) 为方向采样 samplerCube。
 */
class SkyCubeSphereApp : public WorkObject
{
private:
    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    Geometry* sky_geometry = nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};

    TextureCube* sky_cube_texture = nullptr;
    Sampler*     sampler          = nullptr;

private:

    bool InitSkyCubeResource()
    {
        auto* geometry_manager = GetManager<GeometryManager>();
        auto* texture_manager  = GetManager<TextureManager>();
        auto* sampler_manager  = GetManager<SamplerManager>();

        if (!geometry_manager || !texture_manager || !sampler_manager)
            return false;

        // ---- Cubemap 纹理 ----
        sky_cube_texture = texture_manager->LoadTextureCube(
            OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k.TexCube"),
            false);

        if (!sky_cube_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        // ---- 球体几何(屏幕中央,原点处) ----
        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            GetDevice(),
            CreateSkyCubeGeometryVertexFormat());
        if (!pc)
            return false;

        HexSphereCreateInfo hsci;
        hsci.subdivisions = 4;
        hsci.radius       = 4.0f;

        sky_geometry = CreateHexSphere(pc.get(), &hsci);
        if (!sky_geometry)
            return false;

        geometry_manager->Add(sky_geometry);

        sky_recipe.recipe_name = "SkyCubeSphere.Sky";
        sky_recipe.mtl_def_id  = "SkyCube";
        sky_recipe.render_state_overrides.pipeline_config = mtl::MakeSkyConfig();
        sky_asset = PrimitiveAsset(sky_geometry, &sky_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !sky_geometry || !sky_cube_texture || !sampler)
            return false;

        auto* entity = ecs_context->CreateEntity<Entity>("SkyCubeSphere");
        auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
        auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f));
        transform->SetMovable(false);

        primitive_comp->SetPrimitiveAsset(&sky_asset);
        primitive_comp->SetMaterialTextureResource("sky_cube", sky_cube_texture, sampler);
        primitive_comp->SetVisible(true);

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitSkyCubeResource())
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);   // 注视球心
        camera->distance     = 16.0f;
        camera->yaw          = 45.0f;
        camera->pitch        = -10.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.05f, 0.06f, 0.08f, 1.0f));

        if (!InitScene())
            return false;

        return InitCamera();
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<SkyCubeSphereApp>(OS_TEXT("SkyCube Sphere ECS"), argc, argv, 1280, 720);
}
