#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateDomeGeometryVertexFormat()
    {
        // UnlitTexture3D 要求 Position|TexCoord 顶点流。
        // dome_64 穹顶网格顶点按 Lambert Azimuthal 等面积方位投影反算 UV，
        // 纹理直接作为普通 2D 贴图采样，无需着色器分支。
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }
}

/**
 * Lambert Azimuthal 天空顶示例：
 *
 * 使用 sky_34_cubemap_2k.Tex2D（Lambert Azimuthal 2D 贴图）作为材质，
 * 在场景中央渲染一个由内置 Dome 几何体构建的天空半球穹顶。
 *
 * 材质："SkyLambertDome.Unlit"(ShaderLibrary/asset/recipe/Examples/Environment/
 *       SkyLambertDome.Unlit.recipe.toml)
 *       领域同为 UnlitTexture3D；C++ 侧按 .Tex2D 命中 Texture2D 采样路径。
 */
class SkyLambertDomeApp : public WorkObject
{
private:
    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    Geometry* dome_geometry = nullptr;
    graph::mtl::MaterialRecipe dome_recipe{};
    PrimitiveAsset             dome_asset{};

private:

    bool InitDomeResource()
    {
        // 半球穹顶几何体(原点处，半径 4)
        using namespace inline_geometry;

        const GeometryVertexFormat geometry_vertex_format = CreateDomeGeometryVertexFormat();

        dome_geometry = GetGeometry("geo:builtin:dome_64", &geometry_vertex_format);
        if (!dome_geometry)
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("SkyLambertDome.Unlit");
        if (!loaded_recipe)
            return false;
        dome_recipe = *loaded_recipe;

        // 纹理贴图由配方声明，材质侧统一通过 Bindless 采样
        dome_asset = PrimitiveAsset(dome_geometry, &dome_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !dome_geometry)
            return false;

        auto* entity = ecs_context->CreateEntity<Entity>("SkyLambertDome");
        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
        auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
        hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

        transform.SetLocalPosition(glm::vec3(0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(4.0f));
        transform.SetMobility(Mobility::Static);

        primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&dome_asset);

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitDomeResource())
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 1.5f);   // 注视穹顶中央
        camera->distance     = 16.0f;
        camera->yaw          = 45.0f;
        camera->pitch        = -15.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

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
    return RunFramework<SkyLambertDomeApp>(OS_TEXT("Sky Lambert Dome"), argc, argv, 1280, 720);
}
