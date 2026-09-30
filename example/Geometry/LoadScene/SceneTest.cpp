#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<cstring>
#include<filesystem>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
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

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat(VkFormat normal_format = VF_V2UN8)
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},   // UV RG16F（half×2——4B/顶点）
            {VertexSemantic::Normal,   normal_format}, // 默认 VF_V2UN8，支持 VF_V2HF
        };
        return gvf;
    }
}

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
        using MaterialDataAccessor =
            graph::GlobalSSBODataAccessor;

        GeometryVertexFormat geometry_vertex_format;
        MaterialDataAccessor mtl_data_ssbo_accessors[COLOR_COUNT]{};
    };

    MaterialData solid;
    graph::mtl::MaterialRecipe scene_recipe{};

    struct SceneEntity
    {
        hgl::ecs::Entity *entity = nullptr;
        hgl::ecs::TransformAccessor transform;
        std::shared_ptr<hgl::ecs::PrimitiveComponent>  primitive_comp;
    };

    std::vector<PrimitiveAsset>    scene_assets_;
    std::vector<StaticMeshNode>    scene_nodes_;
    std::vector<int32_t>           scene_root_nodes_;
    std::vector<SceneEntity>       scene_entities_;

private:

    bool InitMaterialForDBS(MaterialData *md)
    {
        if (!md)
            return false;

        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);
        for (uint32_t i = 0; i < color_count; ++i)
        {
            md->mtl_data_ssbo_accessors[i] =
                domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
            if (!md->mtl_data_ssbo_accessors[i])
                return false;

            graph::ssbo::EmissiveSurfaceRow material_data{};
            material_data.color = GetColor4f(TestColor[i], 1.0f);
            if (!md->mtl_data_ssbo_accessors[i].Write(material_data))
                return false;
        }

        return true;
    }

    bool InitSolidMDP()
    {
        solid.geometry_vertex_format = CreateStandardGeometryVertexFormat(VF_V2UN8);
        if (solid.geometry_vertex_format.GetCount() == 0)
            return false;

        return InitMaterialForDBS(&solid);
    }

    bool TryLoadStaticMeshScene()
    {
        using std::filesystem::path;
        using std::filesystem::exists;

        const path scene_path("res/ABeautifulGame.StaticMesh/ABeautifulGame.Scene.scene");
        if (!exists(scene_path))
            return false;

        const path scene_dir = scene_path.parent_path();

        auto *device    = GetDevice();
        auto *geo_mgr   = GetManager<GeometryManager>();
        if (!device || !geo_mgr) return false;

        const OSString pack_path = hgl::ToOSString(scene_path.string());
        const OSString base_dir  = hgl::ToOSString(scene_dir.string());

        scene_recipe.recipe_name = "LoadScene.DebugNormalColor";
        scene_recipe.mtl_def_id = "DebugNormalColor";
        scene_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(scene_recipe.material_ssbo_binding = solid.mtl_data_ssbo_accessors[0].GetGlobalSSBOBinding()).IsValid())
            return false;

        return LoadStaticMeshSceneAsPrimitiveAssets(
            device,
            geo_mgr,
            solid.geometry_vertex_format,
            &scene_recipe,
            pack_path,
            base_dir,
            scene_assets_,
            scene_nodes_,
            scene_root_nodes_);
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if (scene_nodes_.empty())
            return false;

        size_t entity_idx = 0;

        for (const auto &node : scene_nodes_)
        {
            for (int32_t pi : node.primitiveIndices)
            {
                if (pi < 0 || pi >= static_cast<int32_t>(scene_assets_.size()))
                    continue;
                PrimitiveAsset &asset = scene_assets_[pi];
                if (!asset.IsValid())
                    continue;

                SceneEntity se;
                se.entity       = ecs_context->CreateEntity<hgl::ecs::Entity>("SceneNode_" + std::to_string(entity_idx++));
                se.transform    = ecs_context->GetTransform(ecs_context->CreateTransform(se.entity->GetEntityID(), hgl::ecs::Mobility::Movable));
                se.primitive_comp = se.entity->AddComponent<hgl::ecs::PrimitiveComponent>();

                // Use pre-computed world matrix for all nodes so child nodes
                // (e.g. Pawn_Top inside Pawn_Body) get the full composed transform.
                //
                // 世界矩阵 → TRS 必须走 math::DecomposeTransform：旧写法
                // quat_cast(glm::mat3(worldMatrix)) 在存在非均匀缩放时输入不是正交基
                // （实测 S=(2,0.5,1)：|q|=1.0768、旋转误差 10.56°、重建误差 0.447），
                // 明细见 src/ecs/support/ProbeTransformDiagnostics.cpp 的 [T4] 段。
                math::Vector3f world_pos;
                math::Quatf    world_rot;
                math::Vector3f world_scale;
                math::DecomposeTransform(node.worldMatrix,world_pos,world_rot,world_scale);

                se.transform.SetLocalPosition(world_pos);
                se.transform.SetLocalRotation(world_rot);
                se.transform.SetLocalScale(world_scale);
                se.transform.SetMobility(hgl::ecs::Mobility::Static);

                se.primitive_comp->SetPrimitiveAsset(&asset);
                hgl::ecs::PrimitiveComponent::MaterialDataAuthoringResource scene_struct{};
                scene_struct =
                    solid.mtl_data_ssbo_accessors[(entity_idx - 1) % COLOR_COUNT].GetGlobalSSBOBinding();
                se.primitive_comp->SetMaterialDataResource(scene_struct);
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
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
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
