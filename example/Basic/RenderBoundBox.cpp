// 该范例主要演示使用ECS架构绘制多个几何体，并渲染对应的包围盒
// This example demonstrates rendering multiple geometries with ECS and drawing their bounding boxes
//
// 本范例展示了：
// 1. 使用ECS架构创建多个实体
// 2. 使用TransformComponent管理空间变换
// 3. 使用PrimitiveComponent管理渲染图元
// 4. 使用AABB生成包围盒实体
// 5. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/GeometryManager.h>

#include<hgl/color/Color.h>
#include<hgl/math/geometry/AABB.h>

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

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

constexpr const COLOR TestColor[]=
{
    COLOR::MozillaCharcoal,
    COLOR::MozillaSand,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BananaYellow,
    COLOR::CherryBlossomPink,

    COLOR::SkyBlue,
    COLOR::GrassGreen,
    COLOR::BloodRed,

    COLOR::Lavender,
    COLOR::Mint,
    COLOR::Coral,

    COLOR::DarkOrange,
    COLOR::DarkTurquoise,
    COLOR::DarkViolet,
};

constexpr const size_t COLOR_COUNT=sizeof(TestColor)/sizeof(COLOR);

class TestApp:public WorkObject
{
private:

    struct RenderMesh
    {
        Geometry *geometry = nullptr;

        Entity *entity = nullptr;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitive_comp;

        ~RenderMesh()
        {
        }
    };

    struct BoundingBoxMesh
    {
        Entity *entity = nullptr;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitive_comp;
    };

    ECSContext *  ecs_context      = nullptr;

    Color4f entity_colors[COLOR_COUNT];

    VertexDataManager *mesh_vdm = nullptr;

    RenderMesh *rm_floor = nullptr;
    std::vector<std::unique_ptr<RenderMesh>> render_mesh;
    std::vector<std::unique_ptr<BoundingBoxMesh>> bounding_boxes;

    Geometry *bbox_geometry = nullptr;

    Entity *camera_entity = nullptr;

    inline static const mtl::MaterialRecipe kSolidCfg {
        .id             = "bounds_solid",
        .preset         = mtl::MaterialPreset::Gizmo3D,
        .vertex_input   = mtl::VertexInputProfile::PositionNormal3D,
        .vertex_policy  = mtl::VertexTransformPolicy::Mesh3D,
        .shading_model  = mtl::SurfaceShadingModel::Gizmo,
        .schema         = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
        .pipeline       = GraphicsPipelinePreset::Solid3D,
    };

    inline static const mtl::MaterialRecipe kWireCfg {
        .id             = "bounds_wire",
        .preset         = mtl::MaterialPreset::PureColor3D,
        .prim           = PrimitiveType::Lines,
        .vertex_input   = mtl::VertexInputProfile::Position3D,
        .vertex_policy  = mtl::VertexTransformPolicy::Mesh3D,
        .shading_model  = mtl::SurfaceShadingModel::PureColor,
        .schema         = mtl::ShaderDataSchema::Color4f,
        .has_explicit_schema = true,
        .pipeline       = GraphicsPipelinePreset::Solid3D,
    };

private:

    bool InitColors()
    {
        for(size_t i = 0; i < COLOR_COUNT; i++)
            entity_colors[i] = GetColor4f(TestColor[i], 1.0f);

        return true;
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetBufferManager();
        if (!buffer_manager)
            return false;

        GeometryVertexFormat gvf;
        gvf.Set(VAN::Position, VF_V3F);
        gvf.Set(VAN::Normal,   VF_V3F);
        mesh_vdm = new VertexDataManager(buffer_manager, gvf);
        if (!mesh_vdm)
            return false;
        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return false;
        return mesh_vdm != nullptr;
    }

    RenderMesh *CreateRenderMesh(Geometry *geometry)
    {
        if(!geometry)
            return nullptr;

        auto rm = std::make_unique<RenderMesh>();
        rm->geometry = geometry;

        RenderMesh *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        auto *graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        auto create_geometry = [this, &geometry_factory](const char *label, auto &&creator) -> Geometry *
        {
            std::cout << "[RenderBoundBox] CreateGeometry START: " << label << std::endl;

            auto pc = geometry_factory.CreateCreater(mesh_vdm);
            if (!pc)
            {
                std::cout << "[RenderBoundBox] CreateGeometry FAIL: GeometryCreater null (" << label << ")" << std::endl;
                return nullptr;
            }

            Geometry *geom = creator(pc.get());
            if (!geom)
            {
                std::cout << "[RenderBoundBox] CreateGeometry FAIL: returned null (" << label << ")" << std::endl;
                return nullptr;
            }

            std::cout << "[RenderBoundBox] CreateGeometry OK: " << label << " geom=" << (void *)geom << std::endl;
            return geom;
        };

        {
            auto geom = create_geometry("Plane", [](GeometryCreater *pc)
            {
                return CreatePlaneSqaure(pc);
            });
            if (!geom)
                return false;

            rm_floor = CreateRenderMesh(geom);
            if (!rm_floor)
                return false;
        }

        {
            auto geom = create_geometry("Sphere", [](GeometryCreater *pc)
            {
                return CreateSphere(pc, 64);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            auto geom = create_geometry("Dome", [](GeometryCreater *pc)
                {
                    DomeCreateInfo dci;
                    dci.number_slices = 64;
                    return CreateDome(pc, &dci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            ConeCreateInfo cci;
            cci.radius      =1;
            cci.halfExtend  =1;
            cci.numberSlices=64;
            cci.numberStacks=4;
            auto geom = create_geometry("Cone", [&](GeometryCreater *pc)
            {
                return CreateCone(pc, &cci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            CylinderCreateInfo cci;
            cci.halfExtend  =1.25f;
            cci.numberSlices=16;
            cci.radius      =1.25f;
            auto geom = create_geometry("Cylinder", [&](GeometryCreater *pc)
            {
                return CreateCylinder(pc, &cci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            TorusCreateInfo tci;
            tci.innerRadius=1.9f;
            tci.outerRadius=2.1f;
            tci.numberSlices=128;
            tci.numberStacks=16;
            auto geom = create_geometry("Torus", [&](GeometryCreater *pc)
            {
                return CreateTorus(pc, &tci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            TubeCreateInfo tci;
            tci.length = 1.25f * 2.0f;
            tci.inner_radius = 0.8f;
            tci.outer_radius = 1.25f;
            tci.segments = 64;
            tci.generate_caps = true;

            auto geom = create_geometry("HollowCylinder", [&](GeometryCreater *pc)
            {
                return CreateTube(pc, &tci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            HexSphereCreateInfo hsci;
            hsci.subdivisions=3;
            auto geom = create_geometry("HexSphere", [&](GeometryCreater *pc)
            {
                return CreateHexSphere(pc, &hsci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            CapsuleCreateInfo cci;
            auto geom = create_geometry("Capsule", [&](GeometryCreater *pc)
            {
                return CreateCapsule(pc, &cci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            TaperedCapsuleCreateInfo tcci;
            tcci.topRadius=0.1f;
            auto geom = create_geometry("TaperedCapsule", [&](GeometryCreater *pc)
            {
                return CreateTaperedCapsule(pc, &tcci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            CubeCreateInfo cci;
            cci.segments_x = 2;
            cci.segments_y = 2;
            cci.segments_z = 2;
            auto geom = create_geometry("Cube", [&](GeometryCreater *pc)
            {
                return CreateCube(pc, &cci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            FrustumCreateInfo fci;
            fci.bottom_radius = 1.0f;
            fci.top_radius = 0.5f;
            fci.height = 2.0f;
            fci.numberSlices = 32;
            auto geom = create_geometry("Frustum", [&](GeometryCreater *pc)
            {
                return CreateFrustum(pc, &fci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        {
            ArrowCreateInfo aci;
            aci.shaft_radius = 0.1f;
            aci.shaft_length = 2.0f;
            aci.head_radius = 0.3f;
            aci.head_length = 0.5f;
            aci.numberSlices = 16;
            aci.cross_section = ArrowCrossSection::Circular;
            auto geom = create_geometry("Arrow", [&](GeometryCreater *pc)
            {
                return CreateArrow(pc, &aci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }

        // 可以运行，但是生成的模型不对劲，有BUG
        // {
        //     RoundedBoxCreateInfo rbci;
        //     rbci.size = Vector3f(1.0f, 1.0f, 1.0f);
        //     rbci.edge_radius = 0.2f;
        //     rbci.edge_segments = 4;
        //     CreateRenderMesh(CreateRoundedBox(prim_creater,&rbci),&solid,13);
        // }

        {
            PipeElbowCreateInfo peci;
            peci.inner_radius = 0.3f;
            peci.outer_radius = 0.5f;
            peci.bend_angle = 90.0f;
            peci.bend_radius = 1.0f;
            peci.pipe_segments = 16;
            peci.bend_segments = 16;
            auto geom = create_geometry("PipeElbow", [&](GeometryCreater *pc)
            {
                return CreatePipeElbow(pc, &peci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom))
                return false;
        }
        return true;
    }

    bool CreateBoundingBoxMesh()
    {

        auto *graphics_context = GetGraphicsContext();
        if (!graphics_context)
            return false;

        GraphicsGeometryFactory geometry_factory(graphics_context);

        GeometryVertexFormat wire_gvf;
        wire_gvf.Set(VAN::Position, VF_V3F);
        auto pc = geometry_factory.CreateCreater(wire_gvf);
        if (!pc)
            return false;

        using namespace inline_geometry;

        inline_geometry::BoundingBoxCreateInfo bbci;
        bbox_geometry = CreateBoundingBox(pc.get(),&bbci);

        if(!bbox_geometry)
            return false;

        if(!geometry_factory.RegisterGeometry(bbox_geometry))
            return false;

        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if(!ecs_context)
            return false;

        if(!CreateGeometryMesh())
            return false;

        if(!CreateBoundingBoxMesh())
            return false;

        if(!InitScene())
            return false;

        if(!InitBoundingBoxScene())
            return false;

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if(!rm_floor)
            return false;

        {
            rm_floor->entity = ecs_context->CreateEntity<Entity>("Floor");
            rm_floor->transform = rm_floor->entity->AddComponent<TransformComponent>(Mobility::Static);
            rm_floor->primitive_comp = rm_floor->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            rm_floor->transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            rm_floor->transform->SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            rm_floor->transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm_floor->transform->SetMovable(false);

            rm_floor->primitive_comp->SetUnresolvedGeometry(rm_floor->geometry);
            rm_floor->primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kSolidCfg), &entity_colors[0], sizeof(Color4f));
            rm_floor->primitive_comp->SetVisible(true);
        }

        const size_t total = render_mesh.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto &rm_ptr : render_mesh)
        {
            auto *rm = rm_ptr.get();
            if(!rm || rm == rm_floor)
                continue;

            rm->entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            rm->transform = rm->entity->AddComponent<TransformComponent>(Mobility::Static);
            rm->primitive_comp = rm->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(mesh_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            rm->transform->SetLocalPosition(pos);
            rm->transform->SetLocalRotation(rotation);
            rm->transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform->SetMovable(false);

            rm->primitive_comp->SetUnresolvedGeometry(rm->geometry);
            rm->primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kSolidCfg), &entity_colors[index % COLOR_COUNT], sizeof(Color4f));
            rm->primitive_comp->SetVisible(true);

            ++index;
        }

        return true;
    }

    bool InitBoundingBoxScene()
    {
        if(!bbox_geometry)
            return false;

        for (size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->entity || !rm->primitive_comp)
                continue;

            hgl::math::AABB local_aabb;
            if(!rm->primitive_comp->GetLocalAABB(local_aabb))
            {
                if (rm->geometry)
                    local_aabb = rm->geometry->GetBoundingVolumes().aabb;

                if (local_aabb.IsEmpty())
                    continue;
            }

            auto bbox = std::make_unique<BoundingBoxMesh>();
            bbox->entity = ecs_context->CreateEntity<Entity>("BBox_" + std::to_string(i));
            bbox->transform = bbox->entity->AddComponent<TransformComponent>(Mobility::Static);
            bbox->primitive_comp = bbox->entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            bbox->transform->SetParent(rm->entity->GetID());

            const auto &center = local_aabb.GetCenter();
            const auto &size = local_aabb.GetLength();

            bbox->transform->SetLocalPosition(glm::vec3(center.x, center.y, center.z));
            bbox->transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            bbox->transform->SetLocalScale(glm::vec3(size.x, size.y, size.z));
            bbox->transform->SetMovable(false);

            bbox->primitive_comp->SetUnresolvedGeometry(bbox_geometry);
            bbox->primitive_comp->SetMaterialRecipe(RegisterMaterialRecipe(kWireCfg), &entity_colors[i % COLOR_COUNT], sizeof(Color4f));
            bbox->primitive_comp->SetVisible(true);

            bounding_boxes.push_back(std::move(bbox));
        }

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
        camera->distance = 12.0f;
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
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f,0.2f,0.2f,1.0f));

        if(!InitColors())
            return false;

        if(!InitVDM())
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

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Render Bounding Box (ECS)"),argc,argv,1280,720);
}


