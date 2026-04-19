#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/module/MaterialRecipeRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<filesystem>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<memory>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;

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
        GeometryVertexFormat gvf;

        MaterialBindingInstance *mi[COLOR_COUNT]{};
    };

    MaterialData solid;

    struct SceneEntity
    {
        hgl::ecs::Entity *entity = nullptr;
        std::shared_ptr<hgl::ecs::TransformComponent> transform;
        std::shared_ptr<hgl::ecs::PrimitiveComponent>  primitive_comp;
    };

    StaticMesh                    *scene_mesh_     = nullptr;
    std::vector<SceneEntity>       scene_entities_;

private:

    bool InitMaterialInstance(MaterialData *md, const mtl::MaterialRecipe &cfg)
    {
        if(!md)
            return(false);

        Color4f color;

        for(size_t i = 0;i < COLOR_COUNT;i++)
        {
            color = GetColor4f(TestColor[i],1.0);

            md->mi[i] = ResolveOrCreateBindingInstance(cfg, &color, sizeof(color));

            if(!md->mi[i])
                return(false);
        }

        md->gvf = GeometryVertexFormat{};
        md->gvf.Set(VAN::Position, VF_V3F);
        md->gvf.Set(VAN::Normal,   VF_V3F);

        if(md->gvf.GetActiveCount() == 0)
            return(false);

        return true;
    }

    bool InitSolidMDP()
    {

        static const mtl::MaterialRecipe kSolidCfg {
            .id       = "scene_gizmo3d",
            .preset   = mtl::MaterialPreset::Gizmo3D,
            .pipeline = GraphicsPipelinePreset::Solid3D,
        };
        return InitMaterialInstance(&solid, kSolidCfg);
    }

    bool TryLoadStaticMeshScene()
    {
        using std::filesystem::path;
        using std::filesystem::exists;

        const path scene_path("res/ABeautifulGame.StaticMesh/ABeautifulGame.Scene.scene");
        if (!exists(scene_path))
            return false;

        const path scene_dir = scene_path.parent_path();

        auto *render_context = GetRenderContext();
        if (!render_context) return false;
        auto *gfx_ctx = render_context->GetGraphicsContext();
        if (!gfx_ctx) return false;
        auto *device    = gfx_ctx->GetDevice();
        auto *geo_mgr   = gfx_ctx->GetGeometryManager();
        if (!device || !geo_mgr) return false;

        const OSString pack_path = hgl::ToOSString(scene_path.string());
        const OSString base_dir  = hgl::ToOSString(scene_dir.string());

        scene_mesh_ = LoadStaticMeshScene(
            device, geo_mgr,
            solid.gvf, solid.mi, (int)COLOR_COUNT,
            pack_path, base_dir);

        return scene_mesh_ != nullptr;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if (!scene_mesh_ || !scene_mesh_->HasSceneTree())
            return false;

        const auto &nodes     = scene_mesh_->GetNodes();
        const auto &prim_list = scene_mesh_->GetPrimitiveList();
        size_t entity_idx = 0;

        for (const auto &node : nodes)
        {
            for (int32_t pi : node.primitiveIndices)
            {
                if (pi < 0 || pi >= prim_list.GetCount())
                    continue;
                Primitive *prim = prim_list[pi];
                if (!prim)
                    continue;

                SceneEntity se;
                se.entity       = ecs_context->CreateEntity<hgl::ecs::Entity>("SceneNode_" + std::to_string(entity_idx++));
                se.transform    = se.entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
                se.primitive_comp = se.entity->AddComponent<hgl::ecs::PrimitiveComponent>();

                // Use pre-computed world matrix for all nodes so child nodes
                // (e.g. Pawn_Top inside Pawn_Body) get the full composed transform.
                se.transform->SetLocalPosition(glm::vec3(node.worldMatrix[3]));
                se.transform->SetLocalRotation(glm::quat_cast(glm::mat3(node.worldMatrix)));
                se.transform->SetLocalScale(glm::vec3(
                    glm::length(glm::vec3(node.worldMatrix[0])),
                    glm::length(glm::vec3(node.worldMatrix[1])),
                    glm::length(glm::vec3(node.worldMatrix[2]))));
                se.transform->SetMovable(false);

                se.primitive_comp->SetPrimitive(prim);
                se.primitive_comp->SetVisible(true);

                scene_entities_.push_back(std::move(se));
            }
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

        if(!InitCamera())
            return false;

        return true;
    }

public:
    ~TestApp()
    {
        scene_entities_.clear();
        delete scene_mesh_;
    }

    bool Init() override
    {
        if(!InitSolidMDP())
            return(false);

        if(!TryLoadStaticMeshScene())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class TestApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Load Scene"),argc,argv,1280,720);
}

