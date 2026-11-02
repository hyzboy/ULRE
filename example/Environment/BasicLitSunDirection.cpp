#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>

#include<hgl/graph/gizmo/SunDirectionControlSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<vector>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/basic_lit_sun_direction.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW = 0;
}

namespace
{
    GeometryVertexFormat CreateSkyMinimalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }

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

#define DRAW_SKY_SPHERE
#define DRAW_GIZMO

class BasicLitSunDirectionApp : public WorkObject
{
private:

    struct MeshEntry
    {
        Geometry* geometry = nullptr;
        // Geometry 由 GeometryAssetRegistry 持有，本结构只做观察（勿在此 delete）
        PrimitiveAsset asset{};
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

#ifdef DRAW_SKY_SPHERE
    Entity* sky_entity = nullptr;
    /// T6b：太阳方向写进**世界私有光源行 0**（sky 已不含光源参数）。
    std::shared_ptr<LightSystem> light_system;
    Geometry* sky_geometry = nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};
#endif//DRAW_SKY_SPHERE

#ifdef DRAW_GIZMO
    std::shared_ptr<SunDirectionControlSystem> sun_gizmo_system;
#endif//DRAW_GIZMO

    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::GlobalSSBOBinding material_data_ssbo_accessor{};
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

    #ifdef DRAW_SKY_SPHERE
        light_system = ecs_context->GetSystem<LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<LightSystem>();

        if (!light_system)
            return false;

        light_system->SetMainLightDirection(math::Vector3f(0.2f, 0.7f, 0.68f));
            #endif//DRAW_SKY_SPHERE

    #ifdef DRAW_GIZMO
        sun_gizmo_system = ecs_context->GetSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterSystem<SunDirectionControlSystem>();

        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetLightSystem(light_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));
    #endif//DRAW_GIZMO

        return true;
    }

#ifdef DRAW_SKY_SPHERE
    bool InitSkySphereResource()
    {
        if (!ecs_context)
            return false;


        // 天空球：独立的顶点布局（非标准几何格式）⇒ 走 format_override
        const GeometryVertexFormat sky_format = CreateSkyMinimalGeometryVertexFormat();
        sky_geometry = GetGeometry("geo:builtin:hex_sphere_sky", &sky_format);
        if (!sky_geometry)
            return false;

        const mtl::MaterialRecipe *loaded_sky_recipe =
            GetRecipe("BasicLitSunDirection.Sky");
        if (!loaded_sky_recipe)
            return false;
        sky_recipe = *loaded_sky_recipe;
        sky_asset = PrimitiveAsset(sky_geometry, &sky_recipe, PrimitiveType::Triangles);

        return true;
    }
#endif//DRAW_SKY_SPHERE

    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_mesh_recipe =
            GetRecipe("BasicLitSunDirection.Lit");
        if (!loaded_mesh_recipe)
            return false;
        mesh_recipe = *loaded_mesh_recipe;

        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, material_data_ssbo_accessor))
            return false;

        return (mesh_recipe.material_ssbo_binding = material_data_ssbo_accessor).IsValid();
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardGeometryVertexFormat());
        if (!mesh_vdm)
            return false;

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return false;

        return true;
    }

    MeshEntry* CreateMeshEntry(Geometry* geometry)
    {
        if (!geometry)
            return nullptr;

        auto mesh = std::make_unique<MeshEntry>();
        mesh->geometry = geometry;
        mesh->asset = PrimitiveAsset(geometry, &mesh_recipe, PrimitiveType::Triangles);

        MeshEntry* result = mesh.get();
        meshes.push_back(std::move(mesh));

        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        // 共享顶点池：一批几何写进同一个 VertexDataManager
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        {
            auto geom = GetGeometry("geo:builtin:plane_square");
            if (!geom)
                return false;

            floor_mesh = CreateMeshEntry(geom);
            if (!floor_mesh)
                return false;
        }

        {
            auto geom = GetGeometry("geo:builtin:sphere_64");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = GetGeometry("geo:builtin:cube_2x2x2");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = GetGeometry("geo:builtin:torus_ring");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

#if 0    // Arrow is not part of the *.geo.toml shape schema yet
        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateArrow(pc, &aci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }
#endif

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !floor_mesh )
            return false;

    #ifdef DRAW_SKY_SPHERE
        if (!sky_geometry)
            return false;
    #endif//
        {
        #ifdef DRAW_SKY_SPHERE
            sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(sky_entity->GetEntityID(), Mobility::Movable));
            auto primitive_comp = sky_entity->GetContext()->GetOrCreateGeometryData(sky_entity);
            hgl::ecs::MaterialData *material_data_comp = sky_entity->GetContext()->GetOrCreateMaterialData(sky_entity);

            transform.SetLocalPosition(glm::vec3(0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&sky_asset);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        #endif//DRAW_SKY_SPHERE
        }

        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform.SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&floor_mesh->asset);
            // 纹理已由配方声明（mesh_recipe.AddTexture），实体端不再重复绑定
            hgl::ecs::MaterialData::MaterialDataAuthoringResource floor_struct{};
            floor_struct = material_data_ssbo_accessor;
            material_data_comp->SetDataResource(floor_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        const size_t total = meshes.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();
            if (!rm || rm == floor_mesh)
                continue;

            auto* entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(mesh_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(rotation);
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            // 纹理已由配方声明（mesh_recipe.AddTexture），实体端不再重复绑定
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = material_data_ssbo_accessor;
            material_data_comp->SetDataResource(mesh_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            ++index;
        }

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitEnvironmentControl())
            return false;

    #ifdef DRAW_SKY_SPHERE
        if (!InitSkySphereResource())
            return false;
    #endif//DRAW_SKY_SPHERE

        if (!InitVDM())
            return false;

        if (!CreateGeometryMesh())
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 16.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~BasicLitSunDirectionApp()
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.10f, 0.12f, 0.16f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitScene())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<BasicLitSunDirectionApp>(OS_TEXT("Standard Sun Direction"), argc, argv, 1280, 720);
}
