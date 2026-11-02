#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/LoadGeometry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<cstring>
#include<hgl/math/geometry/AABB.h>
#include<hgl/type/StdString.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
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

    GeometryVertexFormat CreatePureColorGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }
}

// 颜色数值已搬到数据块文件；这里只保留行数（门禁测试会核对文件行数与它一致）
constexpr const size_t COLOR_COUNT = 9;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 行号，材质数值全部在块文件里
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/load_geometry.material_data.toml";
    constexpr uint32_t MATERIAL_DATA_ROW = 0;
}

class LoadGeometryApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    struct MaterialData
    {
        GeometryVertexFormat geometry_vertex_format;
        graph::GlobalSSBOBinding material_data_ssbo_accessors[COLOR_COUNT]{};
    };

    MaterialData solid;
    MaterialData wire;
    graph::mtl::MaterialRecipe solid_recipe{};
    graph::mtl::MaterialRecipe wire_recipe{};

    struct MeshEntry
    {
        Geometry *geometry;
        PrimitiveAsset asset;
        uint32_t color_index = 0;

        hgl::ecs::Entity *entity = nullptr;
        hgl::ecs::TransformAccessor transform;
        hgl::ecs::GeometryData *primitive_comp = nullptr;

    public:

        ~MeshEntry()
        {
            delete geometry;
        }
    };

    struct BoundingBoxMesh
    {
        hgl::ecs::Entity *entity = nullptr;
        hgl::ecs::TransformAccessor transform;
        hgl::ecs::GeometryData *primitive_comp = nullptr;
    };

    std::vector<std::unique_ptr<MeshEntry>> render_mesh;
    std::vector<std::unique_ptr<BoundingBoxMesh>> bounding_boxes;

    Geometry *bbox_geometry = nullptr;
    PrimitiveAsset bbox_asset;

private:

    bool InitMaterialRuntimeData(MaterialData *md,
                                 const GeometryVertexFormat &gvf)
    {
        if (!md)
            return false;

        md->geometry_vertex_format = gvf;
        if (md->geometry_vertex_format.GetCount() == 0)
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
        const mtl::MaterialRecipe *loaded_solid =
            GetRecipe("LoadGeometry.DebugNormalColor");
        const mtl::MaterialRecipe *loaded_wire = GetRecipe("LoadGeometry.Wire");
        if (!loaded_solid || !loaded_wire)
            return false;
        solid_recipe = *loaded_solid;
        wire_recipe = *loaded_wire;
        return true;
    }

    bool InitSolidMDP()
    {
        if (!InitMaterialRuntimeData(
                &solid,
                CreateStandardGeometryVertexFormat(VF_V2UN8)))
            return false;

        return (solid_recipe.material_ssbo_binding = solid.material_data_ssbo_accessors[0]).IsValid();
    }

    bool InitWireMDP()
    {
        if (!InitMaterialRuntimeData(
                &wire,
                CreatePureColorGeometryVertexFormat()))
            return false;

        return (wire_recipe.material_ssbo_binding = wire.material_data_ssbo_accessors[0]).IsValid();
    }

    bool CreateBoundingBoxMesh()
    {
        auto* device = GetDevice();
        if (!device)
            return false;

        const GeometryVertexFormat geometry_vertex_format = CreatePureColorGeometryVertexFormat();

        bbox_geometry = GetGeometry("geo:builtin:bounding_box", &geometry_vertex_format);
        if(!bbox_geometry)
            return false;

        bbox_asset = PrimitiveAsset(bbox_geometry, &wire_recipe, PrimitiveType::Lines);
        return bbox_asset.IsValid();
    }

    MeshEntry *CreateMeshEntry(Geometry *geometry,const int color)
    {
        if(!geometry)
            return(nullptr);

        auto rm = std::make_unique<MeshEntry>();
        rm->geometry = geometry;
        rm->asset = PrimitiveAsset(geometry, &solid_recipe, PrimitiveType::Triangles);
        if (!rm->asset.IsValid())
            return nullptr;
        rm->color_index = static_cast<uint32_t>(color);

        MeshEntry *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    bool CreateGeometryMesh()
    {
        int count=0;
        const GeometryVertexFormat &geometry_vertex_format = solid.geometry_vertex_format;

//        for(int i=0;i< COLOR_COUNT;i++)
        {
            OSString fn = OSString(OS_TEXT("res/model/vulkan_logo/scene.StaticMesh/scene.geometry"));

            Geometry *geo = LoadGeometry(GetDevice(),geometry_vertex_format,fn);

            if(!geo)
                return(false);

            MeshEntry *rm=CreateMeshEntry(geo,0);

            if(!rm)
            {
                delete geo;
                return(false);
            }

            ++count;
        }

        return(count>0);
    }

    bool InitBoundingBoxScene()
    {
        if(!bbox_asset.IsValid())
            return false;

        for(size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->entity || !rm->primitive_comp)
                continue;

            hgl::math::AABB local_aabb;
            if(!rm->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(rm->primitive_comp->GetOwnerID())->GetLocalAABB(local_aabb))
                continue;

            auto bbox = std::make_unique<BoundingBoxMesh>();
            bbox->entity = ecs_context->CreateEntity<hgl::ecs::Entity>("BBox_" + std::to_string(i));
            bbox->transform = ecs_context->GetTransform(ecs_context->CreateTransform(bbox->entity->GetEntityID(), hgl::ecs::Mobility::Movable));
            bbox->primitive_comp = bbox->entity->GetContext()->GetOrCreateGeometryData(bbox->entity);
            hgl::ecs::MaterialData *material_data_comp = bbox->entity->GetContext()->GetOrCreateMaterialData(bbox->entity);

            bbox->transform.SetParent(ecs_context->GetTransformID(rm->entity->GetEntityID()));

            const auto &center = local_aabb.GetCenter();
            const auto &size = local_aabb.GetLength();

            bbox->transform.SetLocalPosition(glm::vec3(center.x, center.y, center.z));
            bbox->transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            bbox->transform.SetLocalScale(glm::vec3(size.x, size.y, size.z));
            bbox->transform.SetMobility(hgl::ecs::Mobility::Static);

            bbox->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(bbox->primitive_comp->GetOwnerID())->SetPrimitiveAsset(&bbox_asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource bbox_struct{};
            bbox_struct =
                wire.material_data_ssbo_accessors[i % COLOR_COUNT];
            material_data_comp->SetDataResource(bbox_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            bounding_boxes.push_back(std::move(bbox));
        }

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        const size_t mesh_count = render_mesh.empty() ? 1 : render_mesh.size();

        for(size_t i = 0; i < render_mesh.size(); ++i)
        {
            auto *rm = render_mesh[i].get();
            if(!rm || !rm->asset.IsValid())
                continue;

            rm->entity = ecs_context->CreateEntity<hgl::ecs::Entity>("Mesh_" + std::to_string(i));
            rm->transform = ecs_context->GetTransform(ecs_context->CreateTransform(rm->entity->GetEntityID(), hgl::ecs::Mobility::Movable));
            rm->primitive_comp = rm->entity->GetContext()->GetOrCreateGeometryData(rm->entity);
            hgl::ecs::MaterialData *material_data_comp = rm->entity->GetContext()->GetOrCreateMaterialData(rm->entity);

            const float angle = glm::radians(360.0f * static_cast<float>(i) / static_cast<float>(mesh_count));
            const glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::vec3 pos = glm::rotate(rotation, glm::vec3(0.25f, 0.0f, 0.0f));

            rm->transform.SetLocalPosition(pos);
            rm->transform.SetLocalRotation(rotation);
            rm->transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform.SetMobility(hgl::ecs::Mobility::Static);

            rm->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(rm->primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct =
                solid.material_data_ssbo_accessors[rm->color_index];
            material_data_comp->SetDataResource(mesh_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = hgl::ecs::CameraControlMode::ViewModel;
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

        if(!InitBoundingBoxScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    bool Init() override
    {
        if (!InitMaterialRecipes())
            return false;

        if(!InitSolidMDP())
            return(false);

        if(!InitWireMDP())
            return(false);

        if(!CreateGeometryMesh())
            return(false);

        if(!CreateBoundingBoxMesh())
            return(false);

        if(!InitECS())
            return(false);

        return(true);
    }
};//class LoadGeometryApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<LoadGeometryApp>(OS_TEXT("Load Geometry"),argc,argv,1280,720);
}
