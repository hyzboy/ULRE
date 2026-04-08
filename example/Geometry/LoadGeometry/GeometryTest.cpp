#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/color/Color.h>
#include<hgl/math/geometry/AABB.h>
#include<hgl/type/StdString.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<memory>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;

namespace hgl::graph{
Geometry *LoadGeometry(VulkanDevice *device,const VIL *vil,const OSString &filename);
}//namespace hgl::graph

constexpr const COLOR TestColor[] =
{
    COLOR::MozillaCharcoal,
    COLOR::MozillaSand,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BananaYellow,
    COLOR::CherryBlossomPink,

    COLOR::SkyBlue,
};

constexpr const size_t COLOR_COUNT = sizeof(TestColor) / sizeof(COLOR);

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    struct MaterialData
    {
        MaterialTemplate *material = nullptr;
        const VIL *vil = nullptr;

        MaterialInstance *mi[COLOR_COUNT]{};
    };

    MaterialData solid;
    MaterialData wire;

    struct RenderMesh
    {
        Geometry *geometry;
        Primitive *primitive;

        hgl::ecs::Entity *entity = nullptr;
        std::shared_ptr<hgl::ecs::TransformComponent> transform;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> primitive_comp;

    public:

        ~RenderMesh()
        {
        }
    };

    struct BoundingBoxMesh
    {
        hgl::ecs::Entity *entity = nullptr;
        std::shared_ptr<hgl::ecs::TransformComponent> transform;
        std::shared_ptr<hgl::ecs::PrimitiveComponent> primitive_comp;
    };

    std::vector<std::unique_ptr<RenderMesh>> render_mesh;
    std::vector<std::unique_ptr<BoundingBoxMesh>> bounding_boxes;

    Geometry *bbox_geometry = nullptr;
    Primitive *bbox_primitive = nullptr;

private:

    bool InitMaterialInstance(MaterialData *md, const mtl::MaterialAssetRecord &cfg)
    {
        if(!md)
            return(false);

        auto *registry = GetMaterialAssetRegistry();
        if (!registry)
            return false;

        const MaterialDomainHandle handle = registry->Acquire(cfg);
        if (!handle.IsValid())
            return false;

        Color4f color;

        for(size_t i = 0;i < COLOR_COUNT;i++)
        {
            color = GetColor4f(TestColor[i],1.0);

            md->mi[i] = registry->CreateMI(handle, cfg, &color, sizeof(color));

            if(!md->mi[i])
                return(false);

            if (!md->material)
                md->material = md->mi[i]->GetMaterial();
        }

        if (!md->material)
            return false;

        md->vil = md->material->GetDefaultVIL();

        if(!md->vil)
            return(false);

        return true;
    }

    bool InitSolidMDP()
    {

        static const mtl::MaterialAssetRecord kSolidCfg {
            .id       = "geometry_gizmo3d",
            .preset   = mtl::MaterialPreset::Gizmo3D,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };
        return InitMaterialInstance(&solid, kSolidCfg);
    }

    bool InitWireMDP()
    {

        static const mtl::MaterialAssetRecord kWireCfg {
            .id       = "geometry_wire",
            .preset   = mtl::MaterialPreset::PureColor3D,
            .prim     = PrimitiveType::Lines,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };
        return InitMaterialInstance(&wire, kWireCfg);
    }

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        auto* device = GetDevice();
        auto* geometry_manager = GetGeometryManager();
        if (!device || !geometry_manager)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, wire.material->GetDefaultVIL());

        inline_geometry::BoundingBoxCreateInfo bbci;

        bbox_geometry = CreateBoundingBox(pc.get(),&bbci);
        if(!bbox_geometry)
            return false;

        geometry_manager->Add(bbox_geometry);
        auto* primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return false;

        bbox_primitive = primitive_manager->CreatePrimitive(bbox_geometry, wire.mi[5]->ToSlot());
        return bbox_primitive != nullptr;
    }

    RenderMesh *CreateRenderMesh(Geometry *geometry,MaterialData *md,const int color)
    {
        if(!geometry)
            return(nullptr);

        auto* render_context = GetRenderContext();
        if (!render_context)
            return nullptr;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return nullptr;

        auto* primitive_manager = GetPrimitiveManager();
        if (!primitive_manager)
            return nullptr;

        Primitive *primitive = primitive_manager->CreatePrimitive(geometry,md->mi[color]->ToSlot());

        if(!primitive)
            return nullptr;

        auto rm = std::make_unique<RenderMesh>();
        rm->geometry = geometry;
        rm->primitive = primitive;

        RenderMesh *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    bool CreateGeometryMesh()
    {
        int count=0;

        for(int i=0;i< COLOR_COUNT;i++)
        {
            OSString fn = OSString(OS_TEXT("res/model/Chess/ABeautifulGame.")) + OSString::numberOf(i) + OS_TEXT(".geometry");

            Geometry *geo = LoadGeometry(GetDevice(),solid.vil,fn);

            if(!geo)
                continue;

            RenderMesh *rm=CreateRenderMesh(geo,&solid,i);

            if(!rm)
            {
                delete geo;
                continue;
            }

            ++count;
        }

        return(count>0);
    }

    bool InitBoundingBoxScene()
    {
        if(!bbox_primitive)
            return false;

        for(size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->entity || !rm->primitive_comp)
                continue;

            hgl::math::AABB local_aabb;
            if(!rm->primitive_comp->GetLocalAABB(local_aabb))
                continue;

            auto bbox = std::make_unique<BoundingBoxMesh>();
            bbox->entity = ecs_context->CreateEntity<hgl::ecs::Entity>("BBox_" + std::to_string(i));
            bbox->transform = bbox->entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
            bbox->primitive_comp = bbox->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            bbox->transform->SetParent(rm->entity->GetID());

            const auto &center = local_aabb.GetCenter();
            const auto &size = local_aabb.GetLength();

            bbox->transform->SetLocalPosition(glm::vec3(center.x, center.y, center.z));
            bbox->transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            bbox->transform->SetLocalScale(glm::vec3(size.x, size.y, size.z));
            bbox->transform->SetMovable(false);

            bbox->primitive_comp->SetPrimitive(bbox_primitive);
            bbox->primitive_comp->SetMIIDOverride(wire.mi[i % COLOR_COUNT]->GetMIID());
            bbox->primitive_comp->SetVisible(true);

            bounding_boxes.push_back(std::move(bbox));
        }

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        const size_t mesh_count = render_mesh.empty() ? 1 : render_mesh.size();

        for(size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->primitive)
                continue;

            rm->entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Mesh_" + std::to_string(i));
            rm->transform = rm->entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
            rm->primitive_comp = rm->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            const float angle = glm::radians(360.0f * static_cast<float>(i) / static_cast<float>(mesh_count));
            const glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::vec3 pos = glm::rotate(rotation, glm::vec3(0.25f, 0.0f, 0.0f));

            rm->transform->SetLocalPosition(pos);
            rm->transform->SetLocalRotation(rotation);
            rm->transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform->SetMovable(false);

            rm->primitive_comp->SetPrimitive(rm->primitive);
            rm->primitive_comp->SetMIIDOverride(solid.mi[i % COLOR_COUNT]->GetMIID());
            rm->primitive_comp->SetVisible(true);
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 8.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();

        if(!ecs_context)
            return false;

        if(!InitScene())
            return false;

        if(!InitBoundingBoxScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~TestApp()
    {
    }

    bool Init() override
    {
        if(!InitSolidMDP())
            return(false);

        if(!InitWireMDP())
            return(false);

        if(!CreateGeometryMesh())
            return(false);

        if(!CreateBoundingBoxMesh())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Load Geometry"),argc,argv,1280,720);
}

