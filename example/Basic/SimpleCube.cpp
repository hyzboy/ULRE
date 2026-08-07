// 该范例主要演示使用ECS架构绘制一个立方体，并通过ECS CameraSystem使用ViewModel模式
// This example demonstrates rendering a cube with ECS and driving the camera via ViewModel mode
//
// 本范例展示了：
// 1. 使用ECS架构创建立方体实体
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialLibrary.h>

#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_context      =nullptr;
    Entity *      cube_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    Geometry *          geometry        = nullptr;
    graph::SSBOArrayAccessor<Color4f>* mtl_data_ssbo_accessor = nullptr;
    graph::mtl::MaterialRecipe cube_recipe{};
    PrimitiveAsset             cube_asset{};

private:

    bool InitMaterial()
    {
        return true;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return false;

        auto* device = GetDevice();
        if (!device)
            return false;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreateGizmo3DGeometryVertexFormat());

        CubeCreateInfo cci;
        cci.segments_x = 2;
        cci.segments_y = 3;
        cci.segments_z = 4;

        geometry = CreateCube(pc.get(), &cci);

        if(!geometry)
            return false;

        geometry_manager->Add(geometry);
        return true;
    }

    bool InitMISSBO()
    {
        if (!ecs_context)
            ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        auto* domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(graph::mtl::SSBOType::EmissiveSurface, "SimpleCube:EmissiveSurface:MaterialData", 1);
        if (!mtl_data_ssbo_accessor)
            return false;

        (*mtl_data_ssbo_accessor)[0] = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
        mtl_data_ssbo_accessor->Commit();
        return true;
    }
    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        cube_entity = ecs_context->CreateEntity<Entity>("CubeEntity");

        auto transform = cube_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = cube_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        cube_recipe.recipe_name = "SimpleCube.DebugNormalColor";
        cube_recipe.mtl_def_id = "DebugNormalColor";
        cube_recipe.pipeline_config = mtl::MakeSolid3DConfig();
        cube_recipe.domain = "SimpleCube";
        cube_asset = PrimitiveAsset(geometry, &cube_recipe, PrimitiveType::Triangles);
        primitive_comp->SetPrimitiveAsset(&cube_asset);
        hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource named_struct{};
        named_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
        named_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
        named_struct.data_index = 0;
        named_struct.use_data_index = true;
        named_struct.shared_across_instances = true;
        primitive_comp->SetMaterialDataSlotResource(named_struct);
        primitive_comp->SetVisible(true);

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~TestApp()
    {
        SAFE_CLEAR(mtl_data_ssbo_accessor)
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!CreateCubeGeometry())
            return false;

        if(!InitMaterial())
            return false;

        if(!InitMISSBO())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Cube (ECS)"), argc, argv, 1280, 720);
}
