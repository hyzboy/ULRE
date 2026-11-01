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
 * 八面体(Octahedral)球体示例：
 *
 * 与 SkyCubeSphere 唯一的差别是材质输入：用同一份 cubemap 的八面体展开图
 * sky_34_cubemap_2k.TexOct(2D 正方形贴图) 代替 Cubemap，在场景中央渲染同一个球体。
 *
 * 材质："SkyCubeSphereOct.Unlit"(ShaderLibrary/asset/recipe/Examples/Environment/
 *       SkyCubeSphereOct.Unlit.recipe.toml)
 *       领域同为 UnlitTexture3D；C++ 侧按资源后缀判定投影类型，在 frag_options
 *       里选中八面体解码实现，主着色器与采样点代码完全不变。
 */
class SkyCubeSphereOctApp : public WorkObject
{
private:
    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    Geometry* ball_geometry = nullptr;
    graph::mtl::MaterialRecipe ball_recipe{};
    PrimitiveAsset             ball_asset{};

private:

    bool InitOctCubeResource()
    {
        // 球体几何(原点处，半径 4) —— 相机距离 16，完整可见
        using namespace inline_geometry;

        const GeometryVertexFormat geometry_vertex_format = CreateBallGeometryVertexFormat();

        ball_geometry = GetGeometry("geo:builtin:sphere_64", &geometry_vertex_format);
        if (!ball_geometry)
            return false;

        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("SkyCubeSphereOct.Unlit");
        if (!loaded_recipe)
            return false;
        ball_recipe = *loaded_recipe;

        // 八面体展开图由配方文件的 `[[textures]] asset = "file:...TexOct"` 声明，
        // 示例侧不持有 TextureAsset（实体端零纹理调用）。
        // 采样器由 Bindless 预设统一管理，材质侧不再持有 Sampler。
        ball_asset = PrimitiveAsset(ball_geometry, &ball_recipe, PrimitiveType::Triangles);

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !ball_geometry)
            return false;

        auto* entity = ecs_context->CreateEntity<Entity>("SkyCubeSphereOct");
        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
        auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
        hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

        transform.SetLocalPosition(glm::vec3(0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(4.0f));
        transform.SetMobility(Mobility::Static);

        primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&ball_asset);
        // 纹理已由配方文件声明（[[textures]] base_color），实体端不再重复绑定
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitOctCubeResource())
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
    return RunFramework<SkyCubeSphereOctApp>(OS_TEXT("SkyCubeSphere Octahedral"), argc, argv, 1280, 720);
}
