// NOTE (test-only):
// This sample validates the MaterialRecipe authoring entry on the primitive entity.
// It is NOT the final production authoring/runtime pipeline.
//
// Planned production path:
// 1) MaterialRecipe is authored at top-level material definitions.
// 2) ECS/GPUSCENE resolves + materializes runtime data automatically.
// 3) Sample/app code should not manually own final runtime material binding.
//
// 该范例主要演示使用ECS架构绘制一个立方体，并验证图元实体的 MaterialRecipe 入口。
// This example demonstrates rendering a cube with ECS and validating MaterialRecipe ingress.
//
// 本范例展示了：
// 1. 使用ECS架构创建立方体实体
// 2. 使用TransformAccessor管理空间变换
// 3. 使用 GeometryData 管理渲染图元
// 4. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/color/Color.h>

// 引入ECS相关头文件
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
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
    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

class MaterialRecipeEntryApp:public WorkObject
{
private:

    ECSContext *  ecs_context      =nullptr;
    Entity *      cube_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    Geometry *            geometry = nullptr;
    MaterialDataAccessor material_data_ssbo_accessor{};
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

    bool InitMaterialDataSSBO()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return false;

        graph::ssbo::EmissiveSurfaceRow material_data{};
        material_data.color = GetColor4f(COLOR::BlenderAxisBlue, 1.0f);
        if (!material_data_ssbo_accessor.Write(material_data))
            return false;

        return true;
    }
    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        cube_entity = ecs_context->CreateEntity<Entity>("CubeEntity");

        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(cube_entity->GetEntityID(), Mobility::Static));
        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform.SetMobility(Mobility::Static);

        auto primitive_comp = cube_entity->GetContext()->GetOrCreateGeometryData(cube_entity);
        hgl::ecs::MaterialData *material_data_comp = cube_entity->GetContext()->GetOrCreateMaterialData(cube_entity);
        cube_recipe.recipe_name = "Phase2.MaterialRecipeEntry.Cube";
        cube_recipe.mtl_def_id = "DebugNormalColor";
        cube_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(cube_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return false;

        cube_asset = PrimitiveAsset(geometry, &cube_recipe, PrimitiveType::Triangles);
        primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&cube_asset);
        hgl::ecs::MaterialData::MaterialDataAuthoringResource named_struct{};
        named_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
        material_data_comp->SetDataResource(named_struct);
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
        camera->distance = 6.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~MaterialRecipeEntryApp()
    {
        SAFE_CLEAR(geometry)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!CreateCubeGeometry())
            return false;

        if(!InitMaterial())
            return false;

        if(!InitMaterialDataSSBO())
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
    return RunFramework<MaterialRecipeEntryApp>(OS_TEXT("MaterialRecipe Entry Cube (Test)"), argc, argv, 1280, 720);
}
