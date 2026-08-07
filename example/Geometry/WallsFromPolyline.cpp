#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/Wall.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialLibrary.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>

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
#include<memory>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
}

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    ssbo::LitMaterialData material_data;
    graph::mtl::MaterialRecipe wall_recipe{};
    graph::SSBOArrayAccessor<ssbo::LitMaterialData>* mtl_data_ssbo_accessor = nullptr;
    Sampler *sampler = nullptr;
    Texture2D *base_color_texture = nullptr;
    std::unique_ptr<BindlessTextureManager> bindless_texture_manager;

    VertexDataManager *mesh_vdm = nullptr;

    std::vector<PrimitiveAsset> wall_meshes;

public:
    ~TestApp()
    {
        SAFE_CLEAR(sampler)
        SAFE_CLEAR(mesh_vdm)
        SAFE_CLEAR(mtl_data_ssbo_accessor)
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<hgl::ecs::CameraComponent>();

        camera->control_mode = hgl::ecs::CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 14.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<hgl::graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool InitECSScene()
    {
        if(!ecs_context)
            return false;

        for(size_t i = 0; i < wall_meshes.size(); ++i)
        {
            auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Wall_" + std::to_string(i));
            auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
            auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            prim_comp->SetPrimitiveAsset(&wall_meshes[i]);
            prim_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_color_texture, sampler);
            hgl::ecs::PrimitiveComponent::MaterialDataSlotNamedAuthoringResource wall_struct{};
            wall_struct.data_slot_name = graph::mtl::DefaultMaterialDataSlotName;
            wall_struct.ssbo_id = mtl_data_ssbo_accessor->GetSSBOId();
            wall_struct.data_index = 0;
            wall_struct.use_data_index = true;
            wall_struct.shared_across_instances = true;
            prim_comp->SetMaterialDataSlotResource(wall_struct);
            prim_comp->SetVisible(true);
        }

        return true;
    }

    bool Init() override
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();
        if (!texture_manager || !sampler_manager)
            return false;

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return false;

        material_data.base_color = GetColor4f(COLOR::FireBrick);
        material_data.metallic=0;
        material_data.roughness=0.95f;
        material_data.normal_scale=0.35f;
        wall_recipe.recipe_name = "WallsFromPolyline.Lit";
        wall_recipe.mtl_def_id = "Lit";
        wall_recipe.pipeline_config = mtl::MakeSolid3DConfig();
        wall_recipe.domain = "WallsFromPolyline";

        // Allocate SSBO first so the ID is available before UpsertRecipe.
        auto *domain_manager = GetManager<ResourceDomainManager>();
        auto *buffer_manager = GetManager<BufferManager>();
        if (!domain_manager || !buffer_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<ssbo::LitMaterialData>(
            graph::mtl::SSBOType::ClearCoatSurface,
            "WallsFromPolyline:MaterialData",
            1);
        if (!mtl_data_ssbo_accessor)
            return false;

        (*mtl_data_ssbo_accessor)[0] = material_data;
        mtl_data_ssbo_accessor->Commit();

        graph::mtl::UpsertRecipeSSBOAssetBinding(wall_recipe,
                                                 graph::mtl::DefaultMaterialDataSlotName,
                                                 mtl_data_ssbo_accessor->GetSSBOBinding());

        // Standard surface (QUALITY_TIER=Medium) samples TexAlbedo; bind a fallback texture.
        base_color_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_color_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        // Bindless registration deferred until ECS context is ready.

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardGeometryVertexFormat());
        if (!mesh_vdm)
            return false;
        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U16))
            return false;
        if(!mesh_vdm) return false;

        GeometryCreater *pc = new GeometryCreater(mesh_vdm);

        using namespace inline_geometry;

        // Build several different polylines to create a more complex wall world

        // 1) long zig-zag
        {
            std::vector<math::Vector2f> verts;
            verts.push_back(math::Vector2f(-6.0f, -1.0f));
            verts.push_back(math::Vector2f(-4.5f,  1.2f));
            verts.push_back(math::Vector2f(-3.0f, -0.4f));
            verts.push_back(math::Vector2f(-1.5f,  1.6f));
            verts.push_back(math::Vector2f( 0.0f, -0.2f));
            verts.push_back(math::Vector2f( 1.5f,  1.8f));
            verts.push_back(math::Vector2f( 3.0f, -0.6f));

            uint idx[] = {0,1, 1,2, 2,3, 3,4, 4,5, 5,6};

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.18f;
            wci.height = 2.2f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Round;
            wci.uv_tile_v = 1.5f;
            wci.uv_u_repeat_per_unit = 0.8f;

            Geometry *geometry = CreateWallsFromLines2D(pc, &wci);
            if(geometry)
            {
                geometry_manager->Add(geometry);
                wall_meshes.push_back(PrimitiveAsset(geometry, &wall_recipe, PrimitiveType::Triangles));
            }
        }

        // 2) small rectangle loop (closed)
        {
            std::vector<math::Vector2f> verts;
            verts.push_back(math::Vector2f(-2.5f, -3.0f));
            verts.push_back(math::Vector2f(-2.5f, -1.0f));
            verts.push_back(math::Vector2f(-0.5f, -1.0f));
            verts.push_back(math::Vector2f(-0.5f, -3.0f));

            // indices pairs; make it closed by adding last first pair
            uint idx[] = {0,1, 1,2, 2,3, 3,0};

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.25f;
            wci.height = 1.6f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Miter;
            wci.uv_tile_v = 1.0f;
            wci.uv_u_repeat_per_unit = 1.5f;

            Geometry *geometry = CreateWallsFromLines2D(pc, &wci);
            if(geometry)
            {
                geometry_manager->Add(geometry);
                wall_meshes.push_back(PrimitiveAsset(geometry, &wall_recipe, PrimitiveType::Triangles));
            }
        }

        // 3) U-shaped wall
        {
            std::vector<math::Vector2f> verts;
            verts.push_back(math::Vector2f(1.0f, -2.5f));
            verts.push_back(math::Vector2f(1.0f, -0.5f));
            verts.push_back(math::Vector2f(3.0f, -0.5f));
            verts.push_back(math::Vector2f(3.0f, -2.5f));

            uint idx[] = {0,1, 1,2, 2,3}; // open U-shape

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.22f;
            wci.height = 2.5f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Round;
            wci.uv_tile_v = 2.0f;
            wci.uv_u_repeat_per_unit = 0.6f;

            Geometry *geometry = CreateWallsFromLines2D(pc, &wci);
            if(geometry)
            {
                geometry_manager->Add(geometry);
                wall_meshes.push_back(PrimitiveAsset(geometry, &wall_recipe, PrimitiveType::Triangles));
            }
        }

        // 4) irregular polyline cluster
        {
            std::vector<math::Vector2f> verts;
            verts.push_back(math::Vector2f(4.5f, 0.5f));
            verts.push_back(math::Vector2f(5.2f, 1.8f));
            verts.push_back(math::Vector2f(6.0f, 0.9f));
            verts.push_back(math::Vector2f(6.8f, 2.2f));
            verts.push_back(math::Vector2f(7.5f, 0.3f));

            uint idx[] = {0,1, 1,2, 2,3, 3,4};

            WallCreateInfo wci;
            wci.vertices = verts.data();
            wci.vertexCount = (uint)verts.size();
            wci.indices = idx;
            wci.indexCount = sizeof(idx)/sizeof(idx[0]);
            wci.thickness = 0.12f;
            wci.height = 1.8f;
            wci.cornerJoin = WallCreateInfo::CornerJoin::Bevel;
            wci.uv_tile_v = 1.0f;
            wci.uv_u_repeat_per_unit = 1.2f;

            Geometry *geometry = CreateWallsFromLines2D(pc, &wci);
            if(geometry)
            {
                geometry_manager->Add(geometry);
                wall_meshes.push_back(PrimitiveAsset(geometry, &wall_recipe, PrimitiveType::Triangles));
            }
        }

        delete pc;

        ecs_context = GetECSContext();
        if(!ecs_context) return false;

        if(!InitECSScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc,os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("Walls From Polyline Example - Complex"), argc, argv, 1280, 720);
}
