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
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/color/Color.h>
#include<cstring>
#include<cstdlib>
#include<cstdio>
#include<hgl/math/geometry/AABB.h>
#include<cmath>
#include<hgl/type/StdString.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKTextureReadback.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/transform/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>

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

    // ── 台架门控（PBR 棋盘模式，默认开） ──────────────────────────────
    //   LG_LEGACY=1      回退到旧行为：加载 vulkan_logo 场景 + DebugNormalColor 配方
    //   LG_SCENE=<path>  覆盖 pack .model 路径（默认 res/ABeautifulGame.StaticMesh/…）
    //   LG_GEO=<path>    覆盖几何体路径（默认 ABeautifulGame.2.geometry，棋盘）
    //   LG_MAT=<name>    覆盖配方名匹配子串（默认 Chessboard）
    //   LG_DIST/LG_YAW/LG_PITCH  相机覆盖
    //   LG_FRAMES=N      跑 N 帧退出
    //   LG_DUMP=<tag>    第 8 帧帧内回读落原始字节后退出（lgdump_<tag>_WxH_fmt.raw）
    const OSString &ModelPackPath()
    {
        static const OSString path = []
        {
            const char *env = getenv("LG_SCENE");
            if (env && env[0])
                return hgl::ToOSString(env);
            return OSString(OS_TEXT("res/ABeautifulGame.StaticMesh/ABeautifulGame.model"));
        }();
        return path;
    }

    const OSString &BoardGeometryPath()
    {
        static const OSString path = []
        {
            const char *env = getenv("LG_GEO");
            if (env && env[0])
                return hgl::ToOSString(env);
            return OSString(OS_TEXT("res/ABeautifulGame.StaticMesh/ABeautifulGame.2.geometry"));
        }();
        return path;
    }

    // 天光 IBL：与 LoadScene 同一批烘焙贴图与天空配方
    constexpr os_char IBL_TEX_IRR_CUBE[] = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
    constexpr os_char IBL_TEX_PRE_CUBE[] = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");
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

    // PBR 棋盘模式：pack 自装配材质 + 选中的棋盘配方
    bool pack_mode_ = true;
    graph::SelfContainedModelMaterials pack_materials_;
    const graph::mtl::MaterialRecipe *board_recipe_ = nullptr;
    graph::GlobalSSBOBinding board_binding_{};

    std::shared_ptr<hgl::ecs::EnvironmentSystem> environment_system;
    std::shared_ptr<hgl::ecs::LightSystem>       light_system;

    std::string dump_tag;
    uint32_t    lg_frames     = 0;
    uint32_t    lg_frame_seen = 0;
    uint32_t    dump_frame    = 0;

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

    MeshEntry *CreateMeshEntry(Geometry *geometry,const int color,
                               const mtl::MaterialRecipe *recipe = nullptr)
    {
        if(!geometry)
            return(nullptr);

        auto rm = std::make_unique<MeshEntry>();
        rm->geometry = geometry;
        rm->asset = PrimitiveAsset(geometry, recipe ? recipe : &solid_recipe, PrimitiveType::Triangles);
        if (!rm->asset.IsValid())
            return nullptr;
        rm->color_index = static_cast<uint32_t>(color);

        MeshEntry *result = rm.get();
        render_mesh.push_back(std::move(rm));
        return result;
    }

    // pack 自装配：scene/recipe/material_data/tex2d 全取自模型目录（同 LoadScene PBR 模式）
    bool InitPackMaterials()
    {
        auto *graphics = GetGraphicsContext();
        if (!graphics)
            return false;

        if (!graph::BuildSelfContainedModelMaterials(
                *graphics, ModelPackPath(), "ABeautifulGame", pack_materials_))
            return false;

        const char *mat_env = getenv("LG_MAT");
        const std::string needle = mat_env && mat_env[0] ? mat_env : "Chessboard";

        for (auto *r : pack_materials_.table)
        {
            if (r && r->recipe_name.find(needle) != std::string::npos)
            {
                board_recipe_ = r;
                break;
            }
        }
        if (!board_recipe_ && !pack_materials_.table.empty())
        {
            printf("[LGPACK] 未找到包含 \"%s\" 的配方，回退到第 0 行\n", needle.c_str());
            board_recipe_ = pack_materials_.table[0];
        }
        if (!board_recipe_)
        {
            printf("[LGPACK] pack 材质表为空\n");
            return false;
        }

        board_binding_ = board_recipe_->material_ssbo_binding;
        printf("[LGPACK] recipe=%s\n", board_recipe_->recipe_name.c_str());
        return board_binding_.IsValid();
    }

    // PBR 模式的环境光照：主光方向 + 天光 IBL（与 LoadScene 同口径）
    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

        environment_system = ecs_context->GetSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterSystem<hgl::ecs::EnvironmentSystem>();
        if (!environment_system)
            return false;

        light_system = ecs_context->GetSystem<hgl::ecs::LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<hgl::ecs::LightSystem>();
        if (!light_system)
            return false;

        // 太阳方向必须在天空托管之前写进光源行 0：天光调制按当时太阳现算
        light_system->SetMainLightDirection(math::Vector3f(0.2f, 0.7f, 0.68f));

        hgl::ecs::SkyEnvironmentConfig config;
        config.mode         = hgl::ecs::SkyMode::CubeMapIBL;
        config.sky_recipe   = "IBLEnvironment.Sky";
        config.sky_scale    = 1.0f;
        config.track_camera = true;

        config.irradiance_texture = IBL_TEX_IRR_CUBE;
        config.prefilter_texture  = IBL_TEX_PRE_CUBE;

        config.ldr               = hgl::ecs::SkyAmbientLDR::On;
        config.ldr_exposure_base = 2.5f;
        config.ldr_sun_scale     = 1.0f;
        config.ldr_min_ambient   = 0.05f;

        return environment_system->SetupSkyEnvironment(config);
    }

    bool CreateGeometryMesh()
    {
        const GeometryVertexFormat &geometry_vertex_format = solid.geometry_vertex_format;

        if (pack_mode_)
        {
            // 单个几何体（默认棋盘 ABeautifulGame.2.geometry），配 Chessboard PBR 配方
            Geometry *geo = LoadGeometry(GetDevice(), geometry_vertex_format, BoardGeometryPath());
            if (!geo)
                return false;
            return CreateMeshEntry(geo, 0, board_recipe_) != nullptr;
        }

        int count=0;
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

            const float angle = pack_mode_ ? 0.0f
                : glm::radians(360.0f * static_cast<float>(i) / static_cast<float>(mesh_count));
            const glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            const glm::vec3 pos = pack_mode_
                ? glm::vec3(0.0f, 0.0f, 0.0f)
                : glm::rotate(rotation, glm::vec3(0.25f, 0.0f, 0.0f));

            rm->transform.SetLocalPosition(pos);
            rm->transform.SetLocalRotation(rotation);
            rm->transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            rm->transform.SetMobility(hgl::ecs::Mobility::Static);

            rm->primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(rm->primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = pack_mode_
                ? board_binding_
                : solid.material_data_ssbo_accessors[rm->color_index];
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
        // 默认取景 = 包围球自动框定：目标 = 包围盒中心，距离 = 2×包围球半径
        // （棋盘 0.7×0.7×0.018m ⇒ 距离约 1.0，法线微起伏清晰可见）
        hgl::math::Vector3f frame_center(0.0f, 0.0f, 0.0f);
        float frame_radius = -1.0f;
        if (pack_mode_ && !render_mesh.empty())
        {
            auto *rm = render_mesh[0].get();
            if (rm && rm->primitive_comp)
            {
                hgl::math::AABB local_aabb;
                if (rm->primitive_comp->GetOwner()->GetContext()
                        ->GetOrCreateGeometryData(rm->primitive_comp->GetOwnerID())
                        ->GetLocalAABB(local_aabb))
                {
                    frame_center = local_aabb.GetCenter();
                    const auto &l = local_aabb.GetLength();
                    frame_radius = 0.5f * std::sqrt(l.x*l.x + l.y*l.y + l.z*l.z);
                }
            }
        }

        camera->target = frame_center;
        camera->distance = frame_radius > 0.0f ? 2.0f * frame_radius : 8.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        if (const char * v = getenv("LG_DIST"))   camera->distance = (float)atof(v);
        if (const char * v = getenv("LG_YAW"))    camera->yaw      = (float)atof(v);
        if (const char * v = getenv("LG_PITCH"))  camera->pitch    = (float)atof(v);

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
        pack_mode_ = (getenv("LG_LEGACY") == nullptr);

        if (!InitMaterialRecipes())
            return false;

        if(!InitSolidMDP())
            return(false);

        if(!InitWireMDP())
            return(false);

        if (pack_mode_ && !InitPackMaterials())
            return false;

        if(!CreateGeometryMesh())
            return(false);

        if(!CreateBoundingBoxMesh())
            return(false);

        if(!InitECS())
            return(false);

        if (pack_mode_ && !InitEnvironmentControl())
            return false;

        if (const char * tag = getenv("LG_DUMP"))
            dump_tag = tag;

        if (const char * v = getenv("LG_FRAMES"))
            lg_frames = (uint32_t)atoi(v);

        return(true);
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // LG_DUMP=<tag>：第 8 帧（LG_DUMP_FRAME 可覆盖）帧内回读颜色靶，落原始字节后退出
        static const uint32_t s_target_dump_frame = []() {
            if (const char *f = getenv("LG_DUMP_FRAME"))
                return (uint32_t)atoi(f);
            return 8u;
        }();

        if (!dump_tag.empty() && ++dump_frame == s_target_dump_frame)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_context ? ecs_context->GetRenderTarget() : nullptr);
            if (!rt)
            {
                printf("[LGDUMP] no swapchain RT\n");
                std::exit(2);
            }

            std::string tag = dump_tag;
            graph::IRenderTarget * rt_ptr = rt;
            rt->SetInFrameReadbackHook([tag, rt_ptr]()
            {
                std::vector<uint8_t> pixels;
                graph::TextureReadbackInfo info;
                if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                {
                    printf("[LGDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "lgdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[LGDUMP] open %s failed\n", name);
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[LGDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        if (lg_frames > 0 && ++lg_frame_seen >= lg_frames)
        {
            printf("[LoadGeometry] done frames=%u\n", lg_frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};//class LoadGeometryApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<LoadGeometryApp>(OS_TEXT("Load Geometry"),argc,argv,1280,720);
}
