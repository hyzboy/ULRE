#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<cstring>
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

namespace
{
    GeometryVertexFormat CreateGeometryVertexFormatFromVIL(const VIL *vil)
    {
        GeometryVertexFormat gvf;
        if(!vil)
            return gvf;

        const uint32_t count = vil->GetVertexAttribCount();
        const VertexInputFormat *vif_list = vil->GetVIFList();

        for(uint32_t i=0;i<count;i++)
        {
            const VertexInputFormat &vif = vif_list[i];
            gvf.Add(vif.semantic, vif.format, uint8_t(vif.vec_size), uint32_t(vif.stride));
        }

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
        MaterialProgram *material = nullptr;
        const VIL *vil = nullptr;
        GeometryVertexFormat geometry_vertex_format;
        graph::SSBOArrayAccessor<Color4f>* mi_ssbo_accessor = nullptr;
        uint32_t ssbo_count = 0;

        ~MaterialData()
        {
            delete mi_ssbo_accessor;
            mi_ssbo_accessor = nullptr;
        }
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

    bool InitMaterialForDBS(MaterialData *md, const char *tag)
    {
        if (!md || !md->material)
            return false;

        auto *domain_manager = GetManager<ResourceDomainManager>();
        if (!domain_manager)
            return false;

        md->vil = md->material->GetDefaultVIL();
        if (!md->vil)
            return false;

        md->geometry_vertex_format = CreateGeometryVertexFormatFromVIL(md->vil);
        if (md->geometry_vertex_format.GetCount() == 0)
            return false;

        if (md->material->GetMIDataBytes() > 0)
        {
            const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);
            bool has_struct_binding = false;
            graph::mtl::SSBOType ssbo_type = graph::mtl::SSBOType::UserDefined;
            for (const auto &req : md->material->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != graph::mtl::DescriptorSemantic::MaterialInstance)
                    continue;

                has_struct_binding = true;
                ssbo_type = req.ssbo_type;
                break;
            }
            if (!has_struct_binding)
                return false;

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
        }

        return true;
    }

    bool InitSolidMDP()
    {
        auto* material_manager = GetManager<MaterialManager>();
        if (!material_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);
        const auto acquire_mtl3d = static_cast<MaterialProgram *(MaterialManager::*)(const mtl::MaterialPreset, mtl::Material3DCreateConfig *)>(&MaterialManager::AcquireMaterialProgram);
        solid.material = (material_manager->*acquire_mtl3d)(mtl::MaterialPreset::Gizmo3D, &cfg);

        return InitMaterialForDBS(&solid, "LoadScene:SolidMIData");
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

        scene_mesh_ = LoadStaticMeshScene(
            device, geo_mgr,
            solid.geometry_vertex_format, solid.material,
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
                graph::mtl::MaterialRecipe recipe{};
                recipe.recipe_name = "LoadScene.Gizmo3D";
                recipe.shading_model = graph::mtl::ShadingModel::Unlit;
                recipe.preset_hint = static_cast<uint32_t>(graph::mtl::MaterialPreset::Gizmo3D);
                recipe.domain = "LoadScene";
                se.primitive_comp->SetMaterialRecipe(recipe);
                hgl::ecs::PrimitiveComponent::MaterialStructNamedAuthoringResource scene_struct{};
                scene_struct.ssbo_name = graph::mtl::SBS_MaterialInstance.name;
                scene_struct.ssbo_id = solid.mi_ssbo_accessor->GetSSBOId();
                scene_struct.struct_index = (entity_idx - 1) % COLOR_COUNT;
                scene_struct.use_struct_index = true;
                scene_struct.shared_across_instances = true;
                se.primitive_comp->SetMaterialStructResource(scene_struct);
                se.primitive_comp->RequestPipeline(InlinePipeline::Solid3D);
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
