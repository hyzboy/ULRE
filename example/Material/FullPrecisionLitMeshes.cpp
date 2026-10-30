#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

#include<vector>
#include<memory>
#include<cstring>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

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

class FullPrecisionLitMeshesApp : public WorkObject
{
private:

    struct MeshEntry
    {
        Geometry* geometry = nullptr;
        // Geometry 由 GeometryManager 唯一持有，本结构只做观察（勿在此 delete）
        PrimitiveAsset asset{};
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;

    graph::mtl::MaterialRecipe mesh_recipe{};
    MaterialDataAccessor material_data_ssbo_accessor{};
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitMaterial()
    {
        if (!material_data_ssbo_accessor)
            return false;
        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("06c.FullPrecisionLit.Lit");
        if (!loaded_recipe)
            return false;
        mesh_recipe = *loaded_recipe;
        if (!(mesh_recipe.material_ssbo_binding = material_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return false;

        return true;
    }

    bool InitMaterialDataSSBO()
    {
        auto* domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        graph::ssbo::PBRSurfaceRow material_data{};
        material_data.base_color  = Color4f(1.0f);
        material_data.metallic    = 0.08f;
        material_data.roughness   = 0.92f;
        material_data.normal_scale = 0.35f;

        material_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();
        if (!material_data_ssbo_accessor)
            return false;

        if (!material_data_ssbo_accessor.Write(material_data))
            return false;

        return true;
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

        auto* geometry_manager = GetManager<GeometryManager>();
        if (!geometry_manager)
            return nullptr;

        geometry_manager->Add(geometry);

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

        auto create_geometry = [this](auto&& creator) -> Geometry*
        {
            auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
            if (!pc)
                return nullptr;

            return creator(pc.get());
        };

        {
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "plane_square");
            });
            if (!geom)
                return false;

            floor_mesh = CreateMeshEntry(geom);
            if (!floor_mesh)
                return false;
        }

        {
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "sphere_64");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {
            auto geom = create_geometry([](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "dome_64");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "cone");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "cylinder_big");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "torus_ring");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "hollow_cylinder_big");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "hex_sphere");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "capsule");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "tapered_capsule");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "cube");
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateGeometryPreset(pc, "frustum");
            });
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

#if 0    // PipeElbow is not part of the *.geo.toml shape schema yet
        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreatePipeElbow(pc, &peci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }
#endif

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !floor_mesh)
            return false;

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
            floor_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
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
            mesh_struct = material_data_ssbo_accessor.GetGlobalSSBOBinding();
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
        camera->distance = 14.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~FullPrecisionLitMeshesApp()
    {
        // GeometryDataVDM releases its ranges through mesh_vdm when GeometryManager
        // destroys the geometries, so clear that owner before releasing the manager.
        if (auto* geometry_manager = GetManager<GeometryManager>())
            geometry_manager->Release();
        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

        if (!InitMaterialDataSSBO())
            return false;

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
    return RunFramework<FullPrecisionLitMeshesApp>(OS_TEXT("Standard Meshes (Full Precision Vertex Format)"), argc, argv, 1280, 720);
}
