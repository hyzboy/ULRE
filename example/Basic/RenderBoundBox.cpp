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
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/color/Color.h>
#include<hgl/math/geometry/AABB.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

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

    GeometryVertexFormat CreatePureColorGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }
}

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

    struct MaterialData
    {
        graph::SSBOArrayAccessor<Color4f> * mi_ssbo_accessor    = nullptr;
        uint32_t ssbo_count = 0;

        ~MaterialData()
        {
            delete mi_ssbo_accessor;
            mi_ssbo_accessor = nullptr;
        }
    };

    struct RenderMesh
    {
        Geometry *geometry = nullptr;
        PrimitiveAsset asset;

        Entity *entity = nullptr;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitive_comp;
        int color_index = 0;

        ~RenderMesh()
        {
            delete geometry;
        }
    };

    struct BoundingBoxMesh
    {
        Entity *entity = nullptr;
        std::shared_ptr<TransformComponent> transform;
        std::shared_ptr<PrimitiveComponent> primitive_comp;
    };

    ECSContext *  ecs_context      = nullptr;

    MaterialData solid;
    MaterialData wire;
    graph::mtl::MaterialRecipe solid_recipe{};
    graph::mtl::MaterialRecipe wire_recipe{};

    VertexDataManager *mesh_vdm = nullptr;

    RenderMesh *rm_floor = nullptr;           // floor
    std::vector<std::unique_ptr<RenderMesh>> render_mesh;
    std::vector<std::unique_ptr<BoundingBoxMesh>> bounding_boxes;

    Geometry *bbox_geometry = nullptr;
    PrimitiveAsset bbox_asset;

    Entity *camera_entity = nullptr;

private:

    bool InitMaterialForDBS(MaterialData *md, const char *tag, const graph::mtl::SSBOType ssbo_type)
    {
        if (!md)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);
        md->ssbo_count = color_count;
        md->mi_ssbo_accessor = domain_manager->AllocateArrayAccessor<Color4f>(
            ssbo_type,
            tag,
            color_count);
        if (!md->mi_ssbo_accessor)
            return false;

        for (uint32_t i = 0; i < color_count; ++i)
            (*md->mi_ssbo_accessor)[i] = GetColor4f(TestColor[i], 1.0f);
        md->mi_ssbo_accessor->Commit();

        return true;
    }

    void InitMaterialRecipes()
    {
        solid_recipe.recipe_name = "RenderBoundBox.Solid";
        solid_recipe.mtl_def_id = "DebugNormalColor";
        solid_recipe.pipeline_config = mtl::MakeSolid3DConfig();
        solid_recipe.domain = "RenderBoundBox.Solid";

        wire_recipe.recipe_name = "RenderBoundBox.Wire";
        wire_recipe.mtl_def_id = "builtin/pure_color";
        wire_recipe.pipeline_config = mtl::MakeSolid3DConfig();
        wire_recipe.domain = "RenderBoundBox.Wire";
    }

    bool InitSolidMDP()
    {
        return InitMaterialForDBS(&solid, "RenderBoundBox:SolidMIData", graph::mtl::SSBOType::EmissiveSurface);
    }

    bool InitWireMDP()
    {
        return InitMaterialForDBS(&wire, "RenderBoundBox:WireMIData", graph::mtl::SSBOType::EmissiveSurface);
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateGizmo3DGeometryVertexFormat());
        if (!mesh_vdm)
            return false;
        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return false;
        return mesh_vdm != nullptr;
    }

    RenderMesh *CreateRenderMesh(Geometry *geometry,const int color)
    {
        if(!geometry)
            return nullptr;

        auto rm = std::make_unique<RenderMesh>();
        rm->geometry = geometry;
        rm->asset = PrimitiveAsset(geometry, &solid_recipe, PrimitiveType::Triangles);
        if (!rm->asset.IsValid())
            return nullptr;
        rm->color_index = color;

        RenderMesh *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        auto create_geometry = [this](const char *label, auto &&creator) -> Geometry *
        {
            GLogInfo("[RenderBoundBox] CreateGeometry START: %s", label);

            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
            {
                GLogError("[RenderBoundBox] CreateGeometry FAIL: GeometryCreater null (%s)", label);
                return nullptr;
            }

            Geometry *geom = creator(pc.get());
            if (!geom)
            {
                GLogError("[RenderBoundBox] CreateGeometry FAIL: returned null (%s)", label);
                return nullptr;
            }

            GLogInfo("[RenderBoundBox] CreateGeometry OK: %s geom=%p", label, (void *)geom);
            return geom;
        };

        {
            auto geom = create_geometry("Plane", [](GeometryCreater *pc)
            {
                return CreatePlaneSqaure(pc);
            });
            if (!geom)
                return false;

            rm_floor = CreateRenderMesh(geom, 0);
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

            if (!CreateRenderMesh(geom, 1))
                return false;
        }

        {
            auto geom = create_geometry("Dome", [](GeometryCreater *pc)
            {
                return CreateDome(pc, 64);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom, 2))
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

            if (!CreateRenderMesh(geom, 3))
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

            if (!CreateRenderMesh(geom, 4))
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

            if (!CreateRenderMesh(geom, 5))
                return false;
        }

        {
            HollowCylinderCreateInfo hcci;
            hcci.halfExtend    =1.25f;
            hcci.innerRadius   =0.8f;
            hcci.outerRadius   =1.25f;
            hcci.numberSlices  =64;
            auto geom = create_geometry("HollowCylinder", [&](GeometryCreater *pc)
            {
                return CreateHollowCylinder(pc, &hcci);
            });
            if (!geom)
                return false;

            if (!CreateRenderMesh(geom, 6))
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

            if (!CreateRenderMesh(geom, 7))
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

            if (!CreateRenderMesh(geom, 8))
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

            if (!CreateRenderMesh(geom, 9))
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

            if (!CreateRenderMesh(geom, 10))
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

            if (!CreateRenderMesh(geom, 11))
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

            if (!CreateRenderMesh(geom, 12))
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

            if (!CreateRenderMesh(geom, 14))
                return false;
        }
        return true;
    }

    bool CreateBoundingBoxMesh()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        auto pc = std::make_unique<GeometryCreater>(
            device,
            CreatePureColorGeometryVertexFormat());

        inline_geometry::BoundingBoxCreateInfo bbci;
        bbox_geometry = CreateBoundingBox(pc.get(),&bbci);

        if(!bbox_geometry)
            return false;

        geometry_manager->Add(bbox_geometry);
        bbox_asset = PrimitiveAsset(bbox_geometry, &wire_recipe, PrimitiveType::Lines);
        return bbox_asset.IsValid();
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

            rm_floor->primitive_comp->SetPrimitiveAsset(&rm_floor->asset);
            hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource floor_struct{};
            floor_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            floor_struct.ssbo_id = solid.mi_ssbo_accessor->GetSSBOId();
            floor_struct.data_index = rm_floor->color_index;
            floor_struct.use_data_index = true;
            floor_struct.shared_across_instances = true;
            rm_floor->primitive_comp->SetMaterialDataSlotResource(floor_struct);
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

            rm->primitive_comp->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource mesh_struct{};
            mesh_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            mesh_struct.ssbo_id = solid.mi_ssbo_accessor->GetSSBOId();
            mesh_struct.data_index = rm->color_index;
            mesh_struct.use_data_index = true;
            mesh_struct.shared_across_instances = true;
            rm->primitive_comp->SetMaterialDataSlotResource(mesh_struct);
            rm->primitive_comp->SetVisible(true);

            ++index;
        }

        return true;
    }

    bool InitBoundingBoxScene()
    {
        if(!bbox_asset.IsValid())
            return false;

        for (size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->entity || !rm->primitive_comp)
                continue;

            hgl::math::AABB local_aabb;
            if(!rm->primitive_comp->GetLocalAABB(local_aabb))
                continue;

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

            bbox->primitive_comp->SetPrimitiveAsset(&bbox_asset);
            hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource bbox_struct{};
            bbox_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            bbox_struct.ssbo_id = wire.mi_ssbo_accessor->GetSSBOId();
            bbox_struct.data_index = 5;
            bbox_struct.use_data_index = true;
            bbox_struct.shared_across_instances = true;
            bbox->primitive_comp->SetMaterialDataSlotResource(bbox_struct);
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
        render_mesh.clear();
        rm_floor = nullptr;

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f,0.2f,0.2f,1.0f));
        InitMaterialRecipes();

        if(!InitSolidMDP())
            return false;

        if(!InitWireMDP())
            return false;

        if(!InitVDM())
            return false;

        if(!InitECS())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Render Bounding Box (ECS)"),argc,argv,1280,720);
}
