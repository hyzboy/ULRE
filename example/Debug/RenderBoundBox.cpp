// 该范例主要演示使用ECS架构绘制多个几何体，并渲染对应的包围盒
// This example demonstrates rendering multiple geometries with ECS and drawing their bounding boxes
//
// 本范例展示了：
// 1. 使用ECS架构创建多个实体
// 2. 使用TransformAccessor管理空间变换
// 3. 使用 GeometryData 管理渲染图元
// 4. 使用AABB生成包围盒实体
// 5. CameraSystem配置为ViewModel控制模式

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/log/Log.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialRecipe.h>

#include<hgl/color/Color.h>
#include<hgl/math/geometry/AABB.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/buffer/RenderSceneUBOSystem.h>

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

// 颜色数值已搬到数据块文件；这里只保留行数（门禁测试会核对文件行数与它一致）
constexpr const size_t COLOR_COUNT = 16;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 行号，材质数值全部在块文件里
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/render_bound_box.material_data.toml";
    constexpr uint32_t MATERIAL_DATA_ROW = 0;
}

class RenderBoundBoxApp:public WorkObject
{
private:

    struct MaterialData
    {
        graph::GlobalSSBOBinding material_data_ssbo_accessors[COLOR_COUNT]{};
    };

    struct MeshEntry
    {
        Geometry *geometry = nullptr;
        PrimitiveAsset asset;

        Entity *entity = nullptr;
        hgl::ecs::TransformAccessor transform;
        hgl::ecs::GeometryData *primitive_comp = nullptr;
        int color_index = 0;

        ~MeshEntry()
        {
            delete geometry;
        }
    };

    struct BoundingBoxMesh
    {
        Entity *entity = nullptr;
        hgl::ecs::TransformAccessor transform;
        hgl::ecs::GeometryData *primitive_comp = nullptr;
    };

    ECSContext *  ecs_context      = nullptr;

    MaterialData solid;
    MaterialData wire;
    graph::mtl::MaterialRecipe solid_recipe{};
    graph::mtl::MaterialRecipe wire_recipe{};

    VertexDataManager *mesh_vdm = nullptr;

    MeshEntry *floor_mesh = nullptr;           // floor
    std::vector<std::unique_ptr<MeshEntry>> render_mesh;
    std::vector<std::unique_ptr<BoundingBoxMesh>> bounding_boxes;

    Geometry *bbox_geometry = nullptr;
    PrimitiveAsset bbox_asset;

    Entity *camera_entity = nullptr;

private:

    bool InitMaterialForDBS(MaterialData *md)
    {
        if (!md)
            return false;

        graph::GlobalSSBOBinding first{};
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, first))
            return false;

        // 同一数据块内的相邻行：基行 + 序号，不必逐行查 URI
        const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);
        for (uint32_t i = 0; i < color_count; ++i)
            md->material_data_ssbo_accessors[i] = first.RowOffset(i);

        return true;
    }

    bool InitMaterialRecipes()
    {
        const mtl::MaterialRecipe *loaded_solid = GetRecipe("RenderBoundBox.Solid");
        const mtl::MaterialRecipe *loaded_wire = GetRecipe("RenderBoundBox.Wire");
        if (!loaded_solid || !loaded_wire)
            return false;
        solid_recipe = *loaded_solid;
        wire_recipe = *loaded_wire;
        return true;
    }

    bool InitSolidMDP()
    {
        if (!InitMaterialForDBS(&solid))
            return false;

        return (solid_recipe.material_ssbo_binding = solid.material_data_ssbo_accessors[0]).IsValid();
    }

    bool InitWireMDP()
    {
        if (!InitMaterialForDBS(&wire))
            return false;

        return (wire_recipe.material_ssbo_binding = wire.material_data_ssbo_accessors[0]).IsValid();
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
        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return false;
        return mesh_vdm != nullptr;
    }

    MeshEntry *CreateMeshEntry(Geometry *geometry,const int color)
    {
        if(!geometry)
            return nullptr;

        auto rm = std::make_unique<MeshEntry>();
        rm->geometry = geometry;
        rm->asset = PrimitiveAsset(geometry, &solid_recipe, PrimitiveType::Triangles);
        if (!rm->asset.IsValid())
            return nullptr;
        rm->color_index = color;

        MeshEntry *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        // 共享顶点池：全部网格写进同一个 VertexDataManager
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        static const char *geometry_uri[] =
        {
            "geo:builtin:plane_square",
            "geo:builtin:sphere_64",
            "geo:builtin:dome_64",
            "geo:builtin:cone",
            "geo:builtin:cylinder_big",
            "geo:builtin:torus_ring",
            "geo:builtin:hollow_cylinder_big",
            "geo:builtin:hex_sphere",
            "geo:builtin:capsule",
            "geo:builtin:tapered_capsule",
            "geo:builtin:cube_2x2x2",
            "geo:builtin:frustum",
        };

        static constexpr int geometry_count = int(sizeof(geometry_uri) / sizeof(geometry_uri[0]));

        for (int i = 0; i < geometry_count; ++i)
        {
            Geometry *geom = GetGeometry(geometry_uri[i]);
            if (!geom)
            {
                GLogError("[RenderBoundBox] CreateGeometry FAIL: %s", geometry_uri[i]);
                return false;
            }

            GLogInfo("[RenderBoundBox] CreateGeometry OK: %s geom=%p", geometry_uri[i], (void *)geom);

            MeshEntry *entry = CreateMeshEntry(geom, i);
            if (!entry)
                return false;

            if (i == 0)
                floor_mesh = entry;
        }

        // Arrow is not part of the *.geo.toml shape schema yet -> disabled (same as RoundedBox below)
#if 0
        {
            auto geom = create_geometry("Arrow", [&](GeometryCreater *pc)
            {
                return CreateArrow(pc, &aci);
            });
            if (!geom)
                return false;

            if (!CreateMeshEntry(geom, 12))
                return false;
        }
#endif

        // 可以运行，但是生成的模型不对劲，有BUG
        // {
        //     RoundedBoxCreateInfo rbci;
        //     rbci.size = Vector3f(1.0f, 1.0f, 1.0f);
        //     rbci.edge_radius = 0.2f;
        //     rbci.edge_segments = 4;
        //     CreateMeshEntry(CreateRoundedBox(prim_creater,&rbci),&solid,13);
        // }

        // PipeElbow is not part of the *.geo.toml shape schema yet -> disabled
#if 0
        {
            auto geom = create_geometry("PipeElbow", [&](GeometryCreater *pc)
            {
                return CreatePipeElbow(pc, &peci);
            });
            if (!geom)
                return false;

            if (!CreateMeshEntry(geom, 14))
                return false;
        }
#endif
        return true;
    }

    bool CreateBoundingBoxMesh()
    {
        using namespace inline_geometry;

        // 线框盒：独立的纯色顶点布局（非标准几何格式）⇒ 走 format_override
        const GeometryVertexFormat bbox_format = CreatePureColorGeometryVertexFormat();
        bbox_geometry = GetGeometry("geo:builtin:bounding_box", &bbox_format);
        if (!bbox_geometry)
            return false;

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

        if(!floor_mesh)
            return false;

        {
            floor_mesh->entity = ecs_context->CreateEntity<Entity>("Floor");
            floor_mesh->transform = ecs_context->GetTransform(ecs_context->CreateTransform(floor_mesh->entity->GetEntityID(), Mobility::Static));
            floor_mesh->primitive_comp = floor_mesh->entity->GetContext()->GetOrCreateGeometryData(floor_mesh->entity);
            hgl::ecs::MaterialData *material_data_comp = floor_mesh->entity->GetContext()->GetOrCreateMaterialData(floor_mesh->entity);

            floor_mesh->transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            floor_mesh->transform.SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            floor_mesh->transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            floor_mesh->transform.SetMobility(Mobility::Static);

            floor_mesh->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(floor_mesh->primitive_comp->GetOwnerID())->SetPrimitiveAsset(&floor_mesh->asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource floor_struct{};
            floor_struct =
                solid.material_data_ssbo_accessors[floor_mesh->color_index];
            material_data_comp->SetDataResource(floor_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        const size_t total = render_mesh.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto &rm_ptr : render_mesh)
        {
            auto *rm = rm_ptr.get();
            if(!rm || rm == floor_mesh)
                continue;

            rm->entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            rm->transform = ecs_context->GetTransform(ecs_context->CreateTransform(rm->entity->GetEntityID(), Mobility::Static));
            rm->primitive_comp = rm->entity->GetContext()->GetOrCreateGeometryData(rm->entity);
            hgl::ecs::MaterialData *material_data_comp = rm->entity->GetContext()->GetOrCreateMaterialData(rm->entity);

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(mesh_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            rm->transform.SetLocalPosition(pos);
            rm->transform.SetLocalRotation(rotation);
            rm->transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform.SetMobility(Mobility::Static);

            rm->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(rm->primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct =
                solid.material_data_ssbo_accessors[rm->color_index];
            material_data_comp->SetDataResource(mesh_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

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
            if(!rm->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(rm->primitive_comp->GetOwnerID())->GetLocalAABB(local_aabb))
                continue;

            auto bbox = std::make_unique<BoundingBoxMesh>();
            bbox->entity = ecs_context->CreateEntity<Entity>("BBox_" + std::to_string(i));
            bbox->transform = ecs_context->GetTransform(ecs_context->CreateTransform(bbox->entity->GetEntityID(), Mobility::Static));
            bbox->primitive_comp = bbox->entity->GetContext()->GetOrCreateGeometryData(bbox->entity);
            hgl::ecs::MaterialData *material_data_comp = bbox->entity->GetContext()->GetOrCreateMaterialData(bbox->entity);

            bbox->transform.SetParent(ecs_context->GetTransformID(rm->entity->GetEntityID()));

            const auto &center = local_aabb.GetCenter();
            const auto &size = local_aabb.GetLength();

            bbox->transform.SetLocalPosition(glm::vec3(center.x, center.y, center.z));
            bbox->transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            bbox->transform.SetLocalScale(glm::vec3(size.x, size.y, size.z));
            bbox->transform.SetMobility(Mobility::Static);

            bbox->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(bbox->primitive_comp->GetOwnerID())->SetPrimitiveAsset(&bbox_asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource bbox_struct{};
            bbox_struct = wire.material_data_ssbo_accessors[5];
            material_data_comp->SetDataResource(bbox_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            bounding_boxes.push_back(std::move(bbox));
        }

        return true;
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
        camera->distance = 12.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~RenderBoundBoxApp()
    {
        render_mesh.clear();
        floor_mesh = nullptr;

        // 几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        bbox_geometry = nullptr;

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.2f,0.2f,0.2f,1.0f));
        if (!InitMaterialRecipes())
            return false;

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
    return RunFramework<RenderBoundBoxApp>(OS_TEXT("Render Bounding Box"),argc,argv,1280,720);
}
