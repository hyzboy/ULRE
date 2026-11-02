#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
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
    GeometryVertexFormat CreateBallGeometryVertexFormat()
    {
        // UnlitTexture3D 的契约要求 Position|TexCoord：UV0 既是贴图坐标，
        // 也是 Cubemap / 八面体投影反算方向的唯一输入（见
        // ShaderLibrary/module/provider/material_source/texture_projection/）。
        // sphere_64（经纬度 UV 球）在经度接缝处做了双顶点拆分，可避免插值撕裂。
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
        };
        return gvf;
    }
}

/**
 * Cubemap 球体示例：
 *
 * 使用 sky_34_cubemap_2k.TexCube(Cubemap 纹理) 作为材质，在场景中央渲染一个
 * 由 Cubemap 贴图的球体（看外表面，不是天空球内壁）。
 *
 * 材质："SkyCubeSphere.Unlit"(ShaderLibrary/asset/recipe/Examples/Environment/
 *       SkyCubeSphere.Unlit.recipe.toml)
 *       领域 UnlitTexture3D：无光照、纯纹理；球体等距柱状 uv 反算方向后
 *       采样 samplerCubeArray。投影实现由 frag_options 按资源类型选中，
 *       主着色器不变。
 */
class SkyCubeSphereApp : public WorkObject
{
private:
    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    Geometry* sky_geometry = nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};

private:

    bool InitSkyCubeResource()
    {
        // 球体几何(原点处，半径 4) —— 相机距离 16，完整可见
        using namespace inline_geometry;

        const GeometryVertexFormat geometry_vertex_format = CreateBallGeometryVertexFormat();

        sky_geometry = GetGeometry("geo:builtin:sphere_64", &geometry_vertex_format);
        if (!sky_geometry)
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("SkyCubeSphere.Unlit");
        if (!loaded_recipe)
            return false;
        sky_recipe = *loaded_recipe;

        // 立方体贴图由配方文件的 `[[textures]] asset = "cubefile:..."` 声明，
        // 示例侧不再持有 TextureAsset（实体端零纹理调用）。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
        sky_asset = PrimitiveAsset(sky_geometry, &sky_recipe, PrimitiveType::Triangles);
        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !sky_geometry)
            return false;

        auto* entity = ecs_context->CreateEntity<Entity>("SkyCubeSphere");
        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
        auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
        hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

        transform.SetLocalPosition(glm::vec3(0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(4.0f));
        transform.SetMobility(Mobility::Static);

        primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&sky_asset);
        // 纹理已由配方文件声明（[[textures]] base_color），实体端不再重复绑定
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

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
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);   // 注视球心
        camera->distance     = 16.0f;
        camera->yaw          = 45.0f;
        camera->pitch        = -10.0f;
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
    return RunFramework<SkyCubeSphereApp>(OS_TEXT("SkyCube Sphere"), argc, argv, 1280, 720);
}
