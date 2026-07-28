// NOTE (test-only):
// This sample validates the MaterialRecipe authoring entry on PrimitiveComponent.
// It is NOT the final production authoring/runtime pipeline.
//
// Planned production path:
// 1) MaterialRecipe is authored at top-level material definitions.
// 2) ECS/GPUSCENE resolves + materializes runtime data automatically.
// 3) Sample/app code should not manually own final runtime material binding.
//
// 该范例主要演示使用ECS架构绘制一个立方体，并验证 PrimitiveComponent 的 MaterialRecipe 入口。
// This example demonstrates rendering a cube with ECS and validating PrimitiveComponent MaterialRecipe ingress.
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
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t kMaterialRecipeEntrySsboId = hgl::graph::mtl::MakeRecipeSSBOId(8402);

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
    graph::DeviceBuffer *mi_ssbo        = nullptr;
    graph::mtl::SSBOType material_ssbo_type = graph::mtl::SSBOType::PBRSurface;
    uint32_t             material_ssbo_id = kMaterialRecipeEntrySsboId;
    uint32_t             material_ssbo_count = 0;
    uint32_t             material_ssbo_stride = sizeof(Color4f);
    graph::mtl::MaterialRecipe cube_recipe{};
    PrimitiveAsset             cube_asset{};

private:

    bool InitMaterial()
    {
        if (!geometry)
            return false;

        return true;
    }

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return false;

        auto* device = graphics_context->GetDevice();
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

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        const uint32_t mi_count = 1;
        const VkDeviceSize ssbo_size = static_cast<VkDeviceSize>(mi_count) * material_ssbo_stride;
        material_ssbo_count = mi_count;

        mi_ssbo = domain_manager->EnsureBuffer(graph::mtl::SSBOAddress{material_ssbo_type, material_ssbo_id, 0},
                                               "SimpleCube:PBRSurface:MIData",
                                               ssbo_size,
                                               material_ssbo_count,
                                               SharingMode::Exclusive);
        if (!mi_ssbo)
            return false;

        auto *gpu_buf = mi_ssbo->GetGPUBuffer();
        if (!gpu_buf)
            return false;

        uint8_t *dst = static_cast<uint8_t *>(gpu_buf->Map(0, ssbo_size));
        if (!dst)
            return false;

        memset(dst, 0, static_cast<size_t>(ssbo_size));

        const Color4f color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
        memcpy(dst, &color, material_ssbo_stride);

        gpu_buf->Unmap();

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
        primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
        cube_recipe.recipe_name = "Phase2.MaterialRecipeEntry.Cube";
        cube_recipe.shading_model = graph::mtl::ShadingModel::Unlit;
        cube_recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::Gizmo3D);
        cube_recipe.domain = "Phase2AuthoringTest";
        graph::mtl::UpsertRecipeSSBOAssetBinding(cube_recipe,
                                                 graph::mtl::SBS_MaterialInstance.name,
                                                 material_ssbo_type,
                                                 material_ssbo_id);
        cube_asset = PrimitiveAsset(geometry, &cube_recipe, PrimitiveType::Triangles);
        primitive_comp->SetPrimitiveAsset(&cube_asset);
        hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource named_struct{};
        named_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
        named_struct.ssbo_id = material_ssbo_id;
        named_struct.struct_index = 0;
        named_struct.use_struct_index = false;
        named_struct.shared_across_instances = true;
        primitive_comp->SetMaterialStructResource(named_struct);
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
        SAFE_CLEAR(mi_ssbo)
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

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("MaterialRecipe Entry Cube (Test)"), argc, argv, 1280, 720);
}
