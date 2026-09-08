// MixedBufferMeshes — BasicLitMeshes 的变体：
// 前半（7 个）Geometry 用共享 VertexDataManager（VDM）大缓冲池创建（同 BasicLitMeshes），
// 后半（7 个）Geometry 用每几何独立的私有缓冲创建（同 RenderToTexture/MaterialRecipeEntry），
// 两批使用完全同一套材质（同一 MaterialRecipe / 同一材质 SSBO 行 / 同一组纹理）。
// 用途：验证 ECS 渲染在同一材质 batch 内混绘 VDM 池几何与私有缓冲几何。

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/SamplerManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
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
        PrimitiveAsset asset{};

        ~MeshEntry()
        {
            delete geometry;
        }
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

    graph::mtl::MaterialRecipe mesh_recipe{};
    graph::SSBOArrayAccessor<graph::ssbo::PBRSurfaceRow>* material_data_ssbo_accessor = nullptr;
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    Texture2D* base_texture = nullptr;
    Texture2D* normal_texture = nullptr;
    Texture2D* roughness_texture = nullptr;
    Sampler* sampler = nullptr;

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        auto* sampler_manager = GetManager<SamplerManager>();

        if (!texture_manager || !sampler_manager )
            return false;
        mesh_recipe.recipe_name = "06b.BasicLit.Lit";
        mesh_recipe.mtl_def_id = "Lit";
        mesh_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        graph::mtl::UpsertRecipeSSBOAssetBinding(mesh_recipe,
                                                 graph::mtl::DefaultMaterialPrivateDataSlotName,
                                                 material_data_ssbo_accessor->GetSSBOBinding());

        base_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Albedo.Tex2D"), true);
        if (!base_texture)
            return false;

        normal_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Normal.Tex2D"), true);
        if (!normal_texture)
            return false;

        roughness_texture = texture_manager->LoadTexture2D(OS_TEXT("res/image/Brickwall/Roughness.Tex2D"), true);
        if (!roughness_texture)
            return false;

        sampler = sampler_manager->CreateSampler();
        if (!sampler)
            return false;

        // Bindless registration will be done after ECS systems are ready in InitScene().

        return true;
    }

    bool InitMaterialDataSSBO()
    {
        auto* domain_manager = GetManager<SSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        graph::ssbo::PBRSurfaceRow material_row{};
        material_row.base_color  = Color4f(1.0f);
        material_row.metallic    = 0.08f;
        material_row.roughness   = 0.92f;
        material_row.normal_scale = 0.35f;

        material_data_ssbo_accessor = domain_manager->AllocateArrayAccessor<graph::ssbo::PBRSurfaceRow>(
            "06b:PBRSurface:MaterialData",
            1);
        if (!material_data_ssbo_accessor)
            return false;

        (*material_data_ssbo_accessor)[0] = material_row;
        material_data_ssbo_accessor->Commit();
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

        // 创建并登记一个 mesh 条目：按数据源构造 GeometryCreater（VDM 池切片 /
        // 私有独立缓冲）→ 生成几何 → CreateMeshEntry() 打包资产并注册。
        // 本示例前 7 个几何走 VDM、后 7 个走私有缓冲（同一材质 batch 内两种数据源
        // 混绘——本示例的验证主题）。
        auto create_mesh = [this](MeshBufferSource source, auto&& generator) -> MeshEntry*
        {
            Geometry* geometry = nullptr;

            if (source == MeshBufferSource::VDM)
            {
                auto pc = std::make_unique<GeometryCreater>(mesh_vdm);
                if (pc)
                    geometry = generator(pc.get());
            }
            else
            {
                auto *device = GetDevice();
                if (!device)
                    return nullptr;

                auto pc = std::make_unique<GeometryCreater>(device, CreateStandardGeometryVertexFormat());
                if (pc)
                    geometry = generator(pc.get());
            }

            if (!geometry)
                return nullptr;

            return CreateMeshEntry(geometry);
        };


        // ===== VDM 池组：共享 VertexDataManager 大缓冲（7 个，段偏移寻址） =====
        {
            floor_mesh = create_mesh(MeshBufferSource::VDM, [](GeometryCreater* pc)
            {
                return CreatePlaneSqaure(pc);
            });
            if (!floor_mesh)
                return false;
        }

        {
            if (!create_mesh(MeshBufferSource::VDM, [](GeometryCreater* pc)
            {
                return CreateSphere(pc, 64);
            }))
                return false;
        }

        {
            if (!create_mesh(MeshBufferSource::VDM, [](GeometryCreater* pc)
            {
                return CreateDome(pc, 64);
            }))
                return false;
        }

        {
            ConeCreateInfo cci;
            cci.radius = 1;
            cci.halfExtend = 1;
            cci.numberSlices = 64;
            cci.numberStacks = 4;

            if (!create_mesh(MeshBufferSource::VDM, [&](GeometryCreater* pc)
            {
                return CreateCone(pc, &cci);
            }))
                return false;
        }

        {
            CylinderCreateInfo cci;
            cci.halfExtend = 1.25f;
            cci.numberSlices = 16;
            cci.radius = 1.25f;

            if (!create_mesh(MeshBufferSource::VDM, [&](GeometryCreater* pc)
            {
                return CreateCylinder(pc, &cci);
            }))
                return false;
        }

        {
            TorusCreateInfo tci;
            tci.innerRadius = 1.9f;
            tci.outerRadius = 2.1f;
            tci.numberSlices = 128;
            tci.numberStacks = 16;

            if (!create_mesh(MeshBufferSource::VDM, [&](GeometryCreater* pc)
            {
                return CreateTorus(pc, &tci);
            }))
                return false;
        }

        {
            HollowCylinderCreateInfo hcci;
            hcci.halfExtend = 1.25f;
            hcci.innerRadius = 0.8f;
            hcci.outerRadius = 1.25f;
            hcci.numberSlices = 64;

            if (!create_mesh(MeshBufferSource::VDM, [&](GeometryCreater* pc)
            {
                return CreateHollowCylinder(pc, &hcci);
            }))
                return false;
        }


        // ===== 私有缓冲组：每几何独立 VAB/IBO（7 个，偏移恒 0） =====
        {
            HexSphereCreateInfo hsci;
            hsci.subdivisions = 3;

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateHexSphere(pc, &hsci);
            }))
                return false;
        }

        {
            CapsuleCreateInfo cci;

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateCapsule(pc, &cci);
            }))
                return false;
        }

        {
            TaperedCapsuleCreateInfo tcci;
            tcci.topRadius = 0.1f;

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateTaperedCapsule(pc, &tcci);
            }))
                return false;
        }

        {
            CubeCreateInfo cci;
            cci.segments_x = 1;
            cci.segments_y = 1;
            cci.segments_z = 1;
            cci.ntb = NTBType::Normal;   // 仅法线（不存 tangent——试验完全无切线）

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateCube(pc, &cci);
            }))
                return false;
        }

        {
            FrustumCreateInfo fci;
            fci.bottom_radius = 1.0f;
            fci.top_radius = 0.5f;
            fci.height = 2.0f;
            fci.numberSlices = 32;

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateFrustum(pc, &fci);
            }))
                return false;
        }

        {
            ArrowCreateInfo aci;
            aci.shaft_radius = 0.1f;
            aci.shaft_length = 2.0f;
            aci.head_radius = 0.3f;
            aci.head_length = 0.5f;
            aci.numberSlices = 16;
            aci.cross_section = ArrowCrossSection::Circular;

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreateArrow(pc, &aci);
            }))
                return false;
        }

        {
            PipeElbowCreateInfo peci;
            peci.inner_radius = 0.3f;
            peci.outer_radius = 0.5f;
            peci.bend_angle = 90.0f;
            peci.bend_radius = 1.0f;
            peci.pipe_segments = 16;
            peci.bend_segments = 16;

            if (!create_mesh(MeshBufferSource::Private, [&](GeometryCreater* pc)
            {
                return CreatePipeElbow(pc, &peci);
            }))
                return false;
        }

        return true;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context)
            return false;

        if(floor_mesh)
        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&floor_mesh->asset);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal, normal_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Roughness, roughness_texture, sampler);
            hgl::ecs::PrimitiveComponent::MaterialPrivateDataSlotAuthoringResource floor_authoring{};
            floor_authoring.material_private_data_slot_name = graph::mtl::DefaultMaterialPrivateDataSlotName;
            floor_authoring.ssbo_id = material_data_ssbo_accessor->GetSSBOId();
            floor_authoring.data_index = 0;
            floor_authoring.use_data_index = false;
            floor_authoring.shared_across_instances = false;
            primitive_comp->SetMaterialPrivateDataSlotResource(floor_authoring);
            primitive_comp->SetVisible(true);
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
            auto transform = entity->AddComponent<TransformComponent>(Mobility::Static);
            auto primitive_comp = entity->AddComponent<PrimitiveComponent>();

            float angle = glm::radians(360.0f * static_cast<float>(ring_slot) / static_cast<float>(ring_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            transform->SetLocalPosition(pos);
            transform->SetLocalRotation(rotation);
            transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform->SetMovable(false);

            primitive_comp->SetPrimitiveAsset(&rm->asset);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::BaseColor, base_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Normal, normal_texture, sampler);
            primitive_comp->SetMaterialTextureResource(graph::mtl::TextureSlot::Roughness, roughness_texture, sampler);
            hgl::ecs::PrimitiveComponent::MaterialPrivateDataSlotAuthoringResource mesh_authoring{};
            mesh_authoring.material_private_data_slot_name = graph::mtl::DefaultMaterialPrivateDataSlotName;
            mesh_authoring.ssbo_id = material_data_ssbo_accessor->GetSSBOId();
            mesh_authoring.data_index = 0;
            mesh_authoring.use_data_index = false;
            mesh_authoring.shared_across_instances = false;
            primitive_comp->SetMaterialPrivateDataSlotResource(mesh_authoring);
            primitive_comp->SetVisible(true);

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
        auto camera = camera_entity->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 14.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo*>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    ~MixedBufferMeshesApp()
    {
        SAFE_CLEAR(material_data_ssbo_accessor)
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
