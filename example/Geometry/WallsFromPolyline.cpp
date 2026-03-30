#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/Wall.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;

class TestApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    mtl::StandardMaterialInstance mi_data;

    Material *material = nullptr;
    MaterialInstance *material_instance = nullptr;
    Pipeline *pipeline = nullptr;
    Sampler *sampler = nullptr;
    Texture2D *base_color_texture = nullptr;

    VertexDataManager *mesh_vdm = nullptr;

    std::vector<Primitive*> wall_meshes;

public:
    ~TestApp()
    {
        SAFE_CLEAR(sampler)
        SAFE_CLEAR(mesh_vdm)
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
            Primitive *primitive = wall_meshes[i];
            if(!primitive)
                continue;

            auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Wall_" + std::to_string(i));
            auto transform = entity->AddComponent<hgl::ecs::TransformComponent>(hgl::ecs::Mobility::Movable);
            auto prim_comp = entity->AddComponent<hgl::ecs::PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            prim_comp->SetPrimitive(primitive);
            prim_comp->SetVisible(true);
        }

        return true;
    }

    bool Init() override
    {
        auto* render_context = GetRenderContext();
        if (!render_context)
            return false;

        auto* graphics_context = render_context->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* texture_manager = graphics_context->GetTextureManager();
        auto* sampler_manager = graphics_context->GetSamplerManager();
        if (!material_manager)
            return false;
        if (!texture_manager || !sampler_manager)
            return false;

        auto* geometry_manager = graphics_context->GetGeometryManager();
        if (!geometry_manager)
            return false;

        mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles,
                        mtl::WithCamera::With,
                        mtl::WithLocalToWorld::With,
                        mtl::WithSky::With);

        mi_data.base_color = GetRGBA(COLOR::FireBrick);
        mi_data.metallic=0;
        mi_data.roughness=0.95f;
        mi_data.normal_scale=0.35f;

        material = material_manager->CreateMaterial(mtl::MaterialPreset::Standard, &cfg);
        if(!material) return false;

        // Standard surface (QUALITY_TIER=Medium) samples TexAlbedo; bind a fallback texture.
        base_color_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_color_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        if (!material->BindTextureSampler(mtl::SamplerSlot::BaseColor,
                          base_color_texture,
                          sampler))
            return false;

        material_instance = material_manager->CreateMaterialInstance(material, (VIL *)nullptr, &mi_data);
        if(!material_instance) return false;

        auto* render_target = render_context->GetCurrentRenderTarget();
        auto* render_pass = render_target ? render_target->GetRenderFormat() : nullptr;
        pipeline = render_pass ? render_pass->CreatePipeline(material, InlinePipeline::Solid3D) : nullptr;
        if(!pipeline) return false;

        const VIL *vil = material->GetDefaultVIL();
        auto* buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(buffer_manager, vil);
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
                auto* primitive_manager = graphics_context->GetPrimitiveManager();
                if (!primitive_manager)
                    return false;

                Primitive *primitive = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);
                if(primitive) wall_meshes.push_back(primitive);
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
                auto* primitive_manager = graphics_context->GetPrimitiveManager();
                if (!primitive_manager)
                    return false;

                Primitive *primitive = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);
                if(primitive) wall_meshes.push_back(primitive);
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
                auto* primitive_manager = graphics_context->GetPrimitiveManager();
                if (!primitive_manager)
                    return false;

                Primitive *primitive = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);
                if(primitive) wall_meshes.push_back(primitive);
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
                auto* primitive_manager = graphics_context->GetPrimitiveManager();
                if (!primitive_manager)
                    return false;

                Primitive *primitive = primitive_manager->CreatePrimitive(geometry, material_instance, pipeline);
                if(primitive) wall_meshes.push_back(primitive);
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

