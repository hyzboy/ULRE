// SimpleCylinder.cpp - 基于 SimpleCube.cpp 改为创建 Cylinder 的示例

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>

#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

class TestApp:public WorkObject
{
private:

    ECSContext *  ecs_context      =nullptr;
    Entity *      cylinder_entity    =nullptr;
    Entity *      camera_entity  =nullptr;

    MaterialTemplate *          material        = nullptr;
    SemanticMaterialId  semantic_material_id = 0;

    Primitive *         primitive       = nullptr;
    bool                mi_color_initialized = false;

private:

    bool InitMaterial()
    {
        static const mtl::MaterialAssetRecord kCylinderCfg {
            .id       = "cylinder_main",
            .preset   = mtl::MaterialPreset::Gizmo3D,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };

        semantic_material_id = RegisterSemanticMaterial(kCylinderCfg);
        return semantic_material_id != 0;
    }

    bool CreateCylinderPrimitive()
    {
        using namespace inline_geometry;

        auto* graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        CylinderCreateInfo cci;
        cci.halfExtend   = 0.5f;   // cylinder height = 1.0
        cci.radius       = 1.0f;
        cci.numberSlices = 6;      // very low tessellation (hexagonal prism look)

        auto *registry = graphics_context->GetMaterialAssetRegistry();
        if (!registry)
            return false;

        mtl::MaterialAssetRecord rec;
        if (!registry->QuerySemanticMaterial(semantic_material_id, rec))
            return false;

        MaterialDomainHandle handle = registry->Acquire(rec);
        if (!handle.material)
            return false;

        const VIL *vil = handle.material->GetDefaultVIL();
        if (!vil)
            return false;

        VertexFormatMap format_map;
        for (uint32_t i = 0; i < vil->GetVertexAttribCount(); ++i)
        {
            const auto *cfg = vil->GetConfig(i);
            if (!cfg)
                continue;

            format_map[cfg->attrib] = cfg->format;
        }

        auto *device = graphics_context->GetDevice();
        auto *buffer_manager = graphics_context->GetBufferManager();
        if (!device || !buffer_manager)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, format_map, buffer_manager);
        if (!pc)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);
        Geometry *geometry = CreateCylinder(pc.get(), &cci);
        if (!geometry)
            return false;

        if (!geometry_factory.RegisterGeometry(geometry))
            return false;

        primitive = geometry_factory.CreatePrimitive(geometry, semantic_material_id);
        return primitive != nullptr;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        cylinder_entity = ecs_context->CreateEntity<Entity>("CylinderEntity");

        auto transform = cylinder_entity->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        auto primitive_comp = cylinder_entity->AddComponent<hgl::ecs::PrimitiveComponent>();
        primitive_comp->SetPrimitive(primitive);
        primitive_comp->SetSemanticMaterial(semantic_material_id);
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
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f, 0.2f, 0.2f, 1.0f));

        if(!InitMaterial())
            return false;

        if(!CreateCylinderPrimitive())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

    void Tick(double delta_time) override
    {
        if (!mi_color_initialized && primitive && primitive->GetMIData())
        {
            const Color4f cylinder_color = GetColor4f(COLOR::BlenderAxisRed, 1.0f);
            primitive->WriteMIData(cylinder_color);
            mi_color_initialized = true;
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Simple Cylinder (ECS)"), argc, argv, 1280, 720);
}

