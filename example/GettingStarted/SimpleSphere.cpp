// 该范例主要演示使用ECS架构绘制一个球体，并通过ECS CameraSystem使用ViewModel模式
// This example demonstrates rendering a sphere with ECS and driving the camera via ViewModel mode
//
// 本范例展示了：
// 1. 使用 CreateSphere 创建球体几何体（私有顶点缓冲——每几何独立 VAB/IBO）
// 2. 使用 Lit(PBR) 材质绘制球体（材质数据行 PBRSurfaceRow + 纹理）
// 3. 使用TransformAccessor管理空间变换
// 4. 使用 GeometryData 管理渲染图元
// 5. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>

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
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},   // UV RG16F（half×2——4B/顶点）
            {VertexSemantic::Normal,   VF_V2UN8},   // RG8 最小格式（octahedral uint8——2B/顶点）
        };
        return gvf;
    }

    constexpr uint SPHERE_SLICES = 64;              // 球体切片数
    constexpr float SPHERE_ROTATE_SPEED = 0.35f;    // 球体自转角速度（弧度/秒）
}

class SimpleSphereApp:public WorkObject
{
private:

    ECSContext *  ecs_context    =nullptr;
    Entity *      sphere_entity  =nullptr;
    Entity *      camera_entity  =nullptr;

    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    graph::mtl::MaterialRecipe sphere_recipe{};
    MaterialDataAccessor       material_data_ssbo_accessor{};

    Geometry *          sphere_geometry = nullptr;
    PrimitiveAsset      sphere_asset{};

    Texture2D * base_texture      = nullptr;
    Texture2D * normal_texture    = nullptr;
    Texture2D * roughness_texture = nullptr;
    Sampler *   sampler           = nullptr;

    hgl::ecs::TransformAccessor sphere_transform;

    double elapsed_time = 0.0;

private:

    bool InitMaterialDataSSBO()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return false;

        graph::ssbo::PBRSurfaceRow material_data{};
        material_data.base_color   = Color4f(1.0f);
        material_data.metallic     = 0.08f;
        material_data.roughness    = 0.92f;
        material_data.normal_scale = 0.35f;

        return material_data_ssbo_accessor.Write(material_data);
    }

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();

        if (!texture_manager || !sampler_manager)
            return false;

        sphere_recipe.recipe_name = "SimpleSphere.Lit";
        sphere_recipe.mtl_def_id = "Lit";
        sphere_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(sphere_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return false;

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_texture)
            return false;

        normal_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        if (!normal_texture)
            return false;

        roughness_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
        if (!roughness_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        return true;
    }

    bool CreateSphereGeometry()
    {
        using namespace inline_geometry;

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return false;

        auto* device = GetDevice();
        if (!device)
            return false;

        // 私有顶点缓冲路径：每几何独立 VAB/IBO（非 VDM 共享池）
        auto geometry_creater = std::make_unique<GeometryCreater>(
            device,
            CreateStandardGeometryVertexFormat());

        sphere_geometry = CreateSphere(geometry_creater.get(), SPHERE_SLICES);
        if (!sphere_geometry)
            return false;

        geometry_manager->Add(sphere_geometry);
        return true;
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

        material_data_comp->SetTextureResource("base_color", base_texture, sampler);
        material_data_comp->SetTextureResource("normal", normal_texture, sampler);
        material_data_comp->SetTextureResource("roughness", roughness_texture, sampler);

        hgl::ecs::MaterialData::MaterialDataAuthoringResource sphere_struct{};
        sphere_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
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
    ~SimpleSphereApp()
    {
        SAFE_CLEAR(sphere_geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterialDataSSBO())
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
