// PlaneGrid3D

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/MaterialSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ShaderBufferSources.h>

#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    using MaterialDataID = graph::ActiveArrayView<graph::ssbo::EmissiveSurfaceRow>::DataID;
    static constexpr MaterialDataID InvalidMaterialDataID =
        graph::ActiveArrayView<graph::ssbo::EmissiveSurfaceRow>::InvalidDataID;
    graph::ActiveArrayView<graph::ssbo::EmissiveSurfaceRow> *mtl_data_ssbo_accessor = nullptr;
    MaterialDataID material_data_ids[3] = {
        InvalidMaterialDataID,
        InvalidMaterialDataID,
        InvalidMaterialDataID};

    Geometry *         geom_plane_grid     =nullptr;
    graph::mtl::MaterialRecipe plane_grid_recipe{};
    PrimitiveAsset             plane_grid_asset{};

    bool CreateRenderObject()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        struct PlaneGridCreateInfo pgci;

        pgci.grid_size.Set(32,32);
        pgci.sub_count.Set(8,8);

        pgci.lum=180;
        pgci.sub_lum=255;

        GeometryVertexFormat plane_grid_gvf{
            {VertexSemantic::Position,  VF_V2F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };

        auto pc = std::make_unique<GeometryCreater>(
            device,
            plane_grid_gvf);

        geom_plane_grid=CreatePlaneGrid2D(pc.get(),&pgci);
        if (geom_plane_grid)
            geometry_manager->Add(geom_plane_grid);

        return geom_plane_grid;
    }

    bool Add(const char *name,const MaterialDataID data_id,const glm::quat &rotation)
    {
        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
        auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(rotation);
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(false);

        prim_comp->SetPrimitiveAsset(&plane_grid_asset);
        hgl::ecs::PrimitiveComponent::MaterialPrivateDataSlotAuthoringResource named_struct{};
        named_struct.material_private_data_slot_name = graph::mtl::DefaultMaterialPrivateDataSlotName;
        named_struct.ssbo_type = graph::mtl::MaterialSSBOType::EmissiveSurface;
        named_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
        named_struct.data_index = data_id;
        named_struct.use_data_index = true;
        named_struct.shared_across_instances = true;
        prim_comp->SetMaterialPrivateDataSlotResource(named_struct);
        prim_comp->SetVisible(true);

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        plane_grid_recipe.recipe_name = "PlaneGrid3D.VertexLuminance";
        plane_grid_recipe.mtl_def_id = "VertexLuminance";
        plane_grid_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        plane_grid_recipe.vertex_node_config.input = graph::mtl::VertexInputMode::Vec2Position;
        plane_grid_recipe.vertex_node_config.position_mapping = graph::mtl::PositionMappingMode::LiftXY_XY0;
        plane_grid_recipe.vertex_node_config.orientation = graph::mtl::OrientationMode::World;
        plane_grid_recipe.vertex_node_config.scale = graph::mtl::ScaleMode::World;
        plane_grid_recipe.vertex_node_config.projection = graph::mtl::ProjectionMode::WorldCameraVP;
        if (!graph::mtl::UpsertRecipeSSBOAssetBinding(
                plane_grid_recipe,
                graph::mtl::DefaultMaterialPrivateDataSlotName,
                graph::mtl::MaterialSSBOType::EmissiveSurface,
                mtl_data_ssbo_accessor->GetSSBOId(),
                graph::mtl::DefaultMaterialPrivateDataSlot,
                material_data_ids[0],
                true,
                true))
            return false;
        plane_grid_asset = PrimitiveAsset(geom_plane_grid, &plane_grid_recipe, PrimitiveType::Lines);

        if(!Add("PlaneXY", material_data_ids[0], glm::quat(1.0f, 0.0f, 0.0f, 0.0f)))
            return false;

        const float rot90 = glm::radians(90.0f);
        if(!Add("PlaneYZ", material_data_ids[1], glm::angleAxis(rot90, glm::vec3(0.0f, 1.0f, 0.0f))))
            return false;
        if(!Add("PlaneXZ", material_data_ids[2], glm::angleAxis(rot90, glm::vec3(1.0f, 0.0f, 0.0f))))
            return false;

        return true;
    }

    bool InitMISSBO()
    {
        if (!ecs_context)
            return false;

        auto *domain_manager = GetManager<MaterialSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->GetMaterialDataAccessor<graph::ssbo::EmissiveSurfaceRow>(
            "PlaneGrid3D:MaterialData",
            3);
        if (!mtl_data_ssbo_accessor)
            return false;

        Color4f grid_color = GetColor4f(COLOR::BlenderAxisRed, 1.0f);
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (!mtl_data_ssbo_accessor->AcquireID(material_data_ids[i]))
            {
                while (i > 0)
                {
                    --i;
                    mtl_data_ssbo_accessor->ReleaseID(material_data_ids[i]);
                    material_data_ids[i] = InvalidMaterialDataID;
                }
                return false;
            }

            graph::ssbo::EmissiveSurfaceRow row{};
            row.color = grid_color;
            if (!mtl_data_ssbo_accessor->WriteByID(material_data_ids[i], row))
            {
                mtl_data_ssbo_accessor->ReleaseID(material_data_ids[i]);
                material_data_ids[i] = InvalidMaterialDataID;
                while (i > 0)
                {
                    --i;
                    mtl_data_ssbo_accessor->ReleaseID(material_data_ids[i]);
                    material_data_ids[i] = InvalidMaterialDataID;
                }
                return false;
            }

            grid_color = GetColor4f(COLOR(int(COLOR::BlenderAxisRed) + int(i) + 1), 1.0f);
        }
        mtl_data_ssbo_accessor->Commit();

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
        camera->distance = 48.0f;
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

        if(!InitMISSBO())
            return false;

        if(!InitScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~TestApp()
    {
        if (mtl_data_ssbo_accessor)
        {
            for (const MaterialDataID data_id : material_data_ids)
            {
                if (data_id != InvalidMaterialDataID
                 && mtl_data_ssbo_accessor->IsActiveID(data_id))
                    mtl_data_ssbo_accessor->ReleaseID(data_id);
            }
            mtl_data_ssbo_accessor = nullptr;
        }

        SAFE_CLEAR(geom_plane_grid);
    }

    bool Init() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("PlaneGrid3D"),argc,argv,1280,720);
}
