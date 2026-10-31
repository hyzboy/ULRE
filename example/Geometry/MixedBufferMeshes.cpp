// MixedBufferMeshes — BasicLitMeshes 的变体：
// 前半（7 个）Geometry 用共享 VertexDataManager（VDM）大缓冲池创建（同 BasicLitMeshes），
// 后半（7 个）Geometry 用每几何独立的私有缓冲创建（同 RenderToTexture/MaterialRecipeEntry），
// 两批使用完全同一套材质（同一 MaterialRecipe / 同一材质 SSBO 行 / 同一组纹理）。
// 用途：验证 ECS 渲染在同一材质 batch 内混绘 VDM 池几何与私有缓冲几何。

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
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
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/mixed_buffer_meshes.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW = 0;
}

namespace
{
    // 几何数据源——本示例的主题概念，代码/注释/实体名共用同一术语表：
    //   VDM    —— 共享 VertexDataManager 大缓冲池（BasicLitMeshes 路径）
    //   Private—— 每几何独立私有缓冲（RenderToTexture 路径）
    enum class MeshBufferSource : uint8_t
    {
        VDM,
        Private,
    };

    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},   // UV RG16F（half×2——4B/顶点，发行版）
            {VertexSemantic::Normal,   VF_V2UN8},   // RG8 最小格式（octahedral uint8——2B/顶点）
            // 不存 Tangent——仅 RG8 Normal（试验：完全不存切线）
        };
        return gvf;
    }
}

class MixedBufferMeshesApp : public WorkObject
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

    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::GlobalSSBOBinding material_data_ssbo_accessor{};
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitMaterial()
    {
        if (!material_data_ssbo_accessor)
            return false;
        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("06b.BasicLit.Lit");
        if (!loaded_recipe)
            return false;
        mesh_recipe = *loaded_recipe;
        if (!(mesh_recipe.material_ssbo_binding = material_data_ssbo_accessor).IsValid())
            return false;

        return true;
    }

    bool InitMaterialDataSSBO()
    {
        return GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, material_data_ssbo_accessor);
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

        // 创建并登记一个 mesh 条目：按数据源选择顶点池 —— VDM 池切片走共享创建器，
        // 私有独立缓冲走注册器默认路径（每几何一套私有 VDM/缓冲）。
        // 几何统一按 URI 取得，CreateMeshEntry() 只做资产打包。
        // 本示例前 7 个几何走 VDM、后 5 个走私有缓冲（同一材质 batch 内两种数据源
        // 混绘——本示例的验证主题）。
        auto create_mesh = [this](MeshBufferSource source, const char *uri) -> MeshEntry*
        {
            Geometry* geometry = nullptr;

            if (source == MeshBufferSource::VDM)
            {
                geometry = GetGeometry(uri);
            }
            else
            {
                static const GeometryVertexFormat private_format = CreateStandardGeometryVertexFormat();
                geometry = GetGeometry(uri, &private_format);
            }

            if (!geometry)
                return nullptr;

            return CreateMeshEntry(geometry);
        };


        {
            GeometryCreater pc(mesh_vdm);
            SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

            // ===== VDM 池组：共享 VertexDataManager 大缓冲（7 个，段偏移寻址） =====
            {
                floor_mesh = create_mesh(MeshBufferSource::VDM, "geo:builtin:plane_square");
                if (!floor_mesh)
                    return false;
            }

            {
                if (!create_mesh(MeshBufferSource::VDM, "geo:builtin:sphere_64"))
                    return false;
            }

            {
                if (!create_mesh(MeshBufferSource::VDM, "geo:builtin:dome_64"))
                    return false;
            }

            {

                if (!create_mesh(MeshBufferSource::VDM, "geo:builtin:cone"))
                    return false;
            }

            {

                if (!create_mesh(MeshBufferSource::VDM, "geo:builtin:cylinder_big"))
                    return false;
            }

            {

                if (!create_mesh(MeshBufferSource::VDM, "geo:builtin:torus_ring"))
                    return false;
            }

            {

                if (!create_mesh(MeshBufferSource::VDM, "geo:builtin:hollow_cylinder_big"))
                    return false;
            }


        }

        // ===== 私有缓冲组：每几何独立 VAB/IBO（5 个，偏移恒 0） =====
        {

            if (!create_mesh(MeshBufferSource::Private, "geo:builtin:hex_sphere"))
                return false;
        }

        {

            if (!create_mesh(MeshBufferSource::Private, "geo:builtin:capsule"))
                return false;
        }

        {

            if (!create_mesh(MeshBufferSource::Private, "geo:builtin:tapered_capsule"))
                return false;
        }

        {

            if (!create_mesh(MeshBufferSource::Private, "geo:builtin:cube"))
                return false;
        }

        {

            if (!create_mesh(MeshBufferSource::Private, "geo:builtin:frustum"))
                return false;
        }

#if 0    // Arrow is not part of the *.geo.toml shape schema yet
        {

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateArrow(pc, &aci);
            }))
                return false;
        }
#endif

#if 0    // PipeElbow is not part of the *.geo.toml shape schema yet
        {

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreatePipeElbow(pc, &peci);
            }))
                return false;
        }
#endif

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context)
            return false;

        if(floor_mesh)
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
            hgl::ecs::MaterialData::MaterialDataAuthoringResource floor_authoring{};
            floor_authoring = material_data_ssbo_accessor;
            material_data_comp->SetDataResource(floor_authoring);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        // 环绕地板一圈摆放其余 mesh（VDM 半圈 / Private 半圈，同材质混绘对比）。
        const size_t ring_count = meshes.size() > 1 ? (meshes.size() - 1) : 1;
        size_t ring_slot = 0;          // 环位序号（决定摆放角度）
        size_t vdm_seq = 0;            // 组内序号（实体名后缀，便于 RenderDoc 对号）
        size_t private_seq = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();
            if (!rm || rm == floor_mesh)
                continue;

            const MeshBufferSource source = (rm->geometry && rm->geometry->GetVDM() != nullptr)
                                          ? MeshBufferSource::VDM
                                          : MeshBufferSource::Private;

            std::string mesh_name = (source == MeshBufferSource::VDM) ? "Mesh_VDM_" : "Mesh_Private_";
            mesh_name += std::to_string((source == MeshBufferSource::VDM) ? vdm_seq++ : private_seq++);

            auto* entity = ecs_context->CreateEntity<Entity>(mesh_name);
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            float angle = glm::radians(360.0f * static_cast<float>(ring_slot) / static_cast<float>(ring_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(rotation);
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            // 纹理已由配方声明（mesh_recipe.AddTexture），实体端不再重复绑定
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_authoring{};
            mesh_authoring = material_data_ssbo_accessor;
            material_data_comp->SetDataResource(mesh_authoring);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            ++ring_slot;
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
    ~MixedBufferMeshesApp()
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

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
    return RunFramework<MixedBufferMeshesApp>(OS_TEXT("Mixed VDM/Private Meshes"), argc, argv, 1280, 720);
}
