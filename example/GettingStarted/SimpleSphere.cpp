// 该范例主要演示使用ECS架构绘制一个球体，并通过ECS CameraSystem使用ViewModel模式
// This example demonstrates rendering a sphere with ECS and driving the camera via ViewModel mode
//
// 本范例展示了：
// 1. 用几何资产 URI（geo:builtin:sphere_64）加载球体几何体
// 2. 使用 Lit(PBR) 材质绘制球体（材质数据行 + 纹理）
//    纹理绑定声明在材质配方里（MaterialRecipe::AddTexture），只给文件名/句柄，
//    由 ECS 的 TextureStreamingSystem 后台加载，就绪后自动渲染
//    材质数据行不再硬编码，改为引用 mtldata 数据块文件的第 0 行
// 3. 使用TransformAccessor管理空间变换
// 4. 使用 GeometryData 管理渲染图元
// 5. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialResolver.h>

#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>
#include<memory>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{

    constexpr uint SPHERE_SLICES = 64;              // 球体切片数
    constexpr float SPHERE_ROTATE_SPEED = 0.35f;    // 球体自转角速度（弧度/秒）

    // 材质数据行的唯一来源；示例代码里不再出现任何一个材质数值
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/simple_sphere.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW = 0;
}

class SimpleSphereApp:public WorkObject
{
private:

    ECSContext *  ecs_context    =nullptr;
    Entity *      sphere_entity  =nullptr;
    Entity *      camera_entity  =nullptr;

    graph::mtl::MaterialRecipe sphere_recipe{};
    graph::GlobalSSBOBinding   material_binding{};

    Geometry *          sphere_geometry = nullptr;
    PrimitiveAsset      sphere_asset{};

    hgl::ecs::TransformAccessor sphere_transform;

    double elapsed_time = 0.0;

private:

    /// 一行 URI + 行号换一个可绑定行；数据块内容全在文件里
    bool InitMaterialDataBinding()
    {
        return GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, material_binding);
    }

    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("SimpleSphere.Lit");
        if (!loaded_recipe)
            return false;
        sphere_recipe = *loaded_recipe;
        sphere_recipe.material_ssbo_binding = material_binding;

        return material_binding.IsValid();
    }

    bool CreateSphereGeometry()
    {
        // 走几何资产 URI：内置球体预设 sphere_64，默认即标准顶点格式
        sphere_geometry = GetGeometry("geo:builtin:sphere_64");
        return sphere_geometry != nullptr;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        sphere_entity = ecs_context->CreateEntity<Entity>("SphereEntity");

        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(sphere_entity->GetEntityID(), Mobility::Movable));
        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform.SetMobility(Mobility::Movable);
        sphere_transform = transform;

        auto primitive_comp = sphere_entity->GetContext()->GetOrCreateGeometryData(sphere_entity);
        hgl::ecs::MaterialData *material_data_comp = sphere_entity->GetContext()->GetOrCreateMaterialData(sphere_entity);

        sphere_asset = PrimitiveAsset(sphere_geometry, &sphere_recipe, PrimitiveType::Triangles);
        primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&sphere_asset);

        hgl::ecs::MaterialData::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_binding;
        material_data_comp->SetDataResource(sphere_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 4.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterialDataBinding())
            return false;

        if(!InitMaterial())
            return false;

        if(!CreateSphereGeometry())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed_time += delta_time;

        if(sphere_transform.IsValid())
            sphere_transform.SetLocalRotation(glm::angleAxis(
                static_cast<float>(elapsed_time) * SPHERE_ROTATE_SPEED,
                glm::vec3(0.0f, 1.0f, 0.0f)));

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<SimpleSphereApp>(OS_TEXT("Simple Sphere"), argc, argv, 1280, 720);
}
