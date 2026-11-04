#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/graph/asset/StaticMeshSceneInstance.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）
#include<hgl/ecs/support/cull/GPUCullSystem.h>
#include<cstring>
#include<cstdlib>
#include<cstdio>
#include<filesystem>

// ── 台架门控（默认全关） ────────────────────────────────────────────
//   LS_SCENE=<path>  覆盖 .scene 资产路径（默认 res/ABeautifulGame.StaticMesh/…；
//                    含 meshlets 表的资产已**不支持** ⇒ 加载即显式拒绝）
//   LS_DIST/LS_YAW/LS_PITCH  相机覆盖
//   LS_FRAMES=N      跑 N 帧退出
//   LS_DUMP=<tag>    第 8 帧帧内回读落原始字节后退出（lsdump_<tag>_WxH_fmt.raw）

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
    GeometryVertexFormat CreateStandardGeometryVertexFormat(VkFormat normal_format = VF_V2UN8)
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},   // UV RG16F（half×2——4B/顶点）
            {VertexSemantic::Normal,   normal_format}, // 默认 VF_V2UN8，支持 VF_V2HF
        };
        return gvf;
    }
}

// 颜色数值已搬到数据块文件；这里只保留行数（门禁测试会核对文件行数与它一致）
constexpr const size_t COLOR_COUNT = 8;

// PBR 模式行数：ABeautifulGame 场景 15 个 glTF 材质，
// 行序 = material_index = Scene.json materials[] 顺序。
constexpr const size_t PBR_MATERIAL_COUNT = 15;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 行号，材质数值全部在块文件里
    constexpr char MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/load_scene.material_data.toml";
    constexpr uint32_t MATERIAL_DATA_ROW = 0;

    // PBR 模式数据块（target=PBRSurface，glTF 缺省因子口径）
    constexpr char MATERIAL_DATA_URI_PBR[] =
        "mtldata:file:ShaderLibrary/asset/material_data/load_scene_pbr.material_data.toml";

    // 15 个棋件配方（名字 ↔ ABeautifulGame 材质名；下标即 material_index）
    constexpr const char *CHESS_RECIPE_NAMES[PBR_MATERIAL_COUNT] =
    {
        "LoadScene.Chess.King_Black",
        "LoadScene.Chess.King_White",
        "LoadScene.Chess.Queen_Black",
        "LoadScene.Chess.Queen_White",
        "LoadScene.Chess.Chessboard",
        "LoadScene.Chess.Pawn_Top_White",
        "LoadScene.Chess.Pawn_Body_White",
        "LoadScene.Chess.Pawn_Top_Black",
        "LoadScene.Chess.Pawn_Body_Black",
        "LoadScene.Chess.Castle_Black",
        "LoadScene.Chess.Castle_White",
        "LoadScene.Chess.Knight_Black",
        "LoadScene.Chess.Knight_White",
        "LoadScene.Chess.Bishop_Black",
        "LoadScene.Chess.Bishop_White",
    };

    // 天光 IBL：复用 IBLEnvironment 示例的同一批烘焙贴图与天空配方
    constexpr os_char IBL_TEX_IRR_CUBE[] = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
    constexpr os_char IBL_TEX_PRE_CUBE[] = OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");
}

class LoadSceneApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;

    std::string dump_tag;
    uint32_t    ls_frames      = 0;
    uint32_t    ls_frame_seen  = 0;
    uint32_t    dump_frame     = 0;

    struct MaterialData
    {
        GeometryVertexFormat geometry_vertex_format;
        // 两套行绑定：调试法线（EmissiveSurface，8 行）与 PBR（PBRSurface，15 行）
        graph::GlobalSSBOBinding debug_rows[COLOR_COUNT]{};
        graph::GlobalSSBOBinding pbr_rows[PBR_MATERIAL_COUNT]{};
    };

    MaterialData solid;
    graph::mtl::MaterialRecipe scene_recipe{};
    // PBR 模式：按 material_index 索引的配方表（指向注册表常驻实例）
    std::vector<const mtl::MaterialRecipe *> chess_recipes_;
    bool pbr_mode_ = true;

    std::shared_ptr<hgl::ecs::EnvironmentSystem> environment_system;
    std::shared_ptr<hgl::ecs::LightSystem>       light_system;

    std::vector<PrimitiveAsset>    scene_assets_;
    std::vector<StaticMeshNode>    scene_nodes_;
    std::vector<int32_t>           scene_root_nodes_;
    // 场景 → ECS 标准实例化器：逐节点局部 TRS + SetParent 还原层级，
    // 自持资产副本（不再需要 DecomposeTransform 摊平，也不惧 vector 搬迁悬垂）。
    StaticMeshSceneInstance        scene_instance_;

private:

    bool InitMaterialRows(const char *uri, uint32_t count, graph::GlobalSSBOBinding *rows)
    {
        if (!uri || !rows || count == 0)
            return false;

        graph::GlobalSSBOBinding first{};
        if (!GetMaterialDataBinding(uri, MATERIAL_DATA_ROW, first))
            return false;

        // 同一数据块内的相邻行：基行 + 序号，不必逐行查 URI
        for (uint32_t i = 0; i < count; ++i)
            rows[i] = first.RowOffset(i);

        return true;
    }

    bool InitSolidMDP()
    {
        solid.geometry_vertex_format = CreateStandardGeometryVertexFormat(VF_V2UN8);
        if (solid.geometry_vertex_format.GetCount() == 0)
            return false;

        if (!InitMaterialRows(MATERIAL_DATA_URI, static_cast<uint32_t>(COLOR_COUNT), solid.debug_rows))
            return false;

        if (pbr_mode_ &&
            !InitMaterialRows(MATERIAL_DATA_URI_PBR, static_cast<uint32_t>(PBR_MATERIAL_COUNT), solid.pbr_rows))
            return false;

        return true;
    }

    bool TryLoadStaticMeshScene()
    {
        using std::filesystem::path;
        using std::filesystem::exists;

        // 资产路径可覆盖（默认不变）。
        const char * scene_env = getenv("LS_SCENE");
        const path scene_path(scene_env ? scene_env
                                        : "res/ABeautifulGame.StaticMesh/ABeautifulGame.Scene.scene");
        if (!exists(scene_path))
            return false;

        const path scene_dir = scene_path.parent_path();

        auto *device    = GetDevice();
        auto *geo_mgr   = GetManager<GeometryManager>();
        if (!device || !geo_mgr) return false;

        const OSString pack_path = hgl::ToOSString(scene_path.string());
        const OSString base_dir  = hgl::ToOSString(scene_dir.string());

        // PBR 模式：15 个棋件配方按 material_index 建表交给装载器；
        // 调试模式：沿用单一 DebugNormal 配方 + 数据行绑定（旧行为）。
        const mtl::MaterialRecipe *default_recipe = nullptr;
        const mtl::MaterialRecipe *const *recipe_table = nullptr;
        uint32_t recipe_table_count = 0;

        if (pbr_mode_)
        {
            chess_recipes_.clear();
            chess_recipes_.reserve(PBR_MATERIAL_COUNT);
            for (uint32_t i = 0; i < PBR_MATERIAL_COUNT; ++i)
            {
                const mtl::MaterialRecipe *r = GetRecipe(CHESS_RECIPE_NAMES[i]);
                if (!r)
                    return false;
                chess_recipes_.push_back(r);
            }
            default_recipe    = chess_recipes_[0];
            recipe_table      = chess_recipes_.data();
            recipe_table_count = static_cast<uint32_t>(chess_recipes_.size());
        }
        else
        {
            const mtl::MaterialRecipe *loaded_recipe =
                GetRecipe("LoadScene.DebugNormalColor");
            if (!loaded_recipe)
                return false;
            scene_recipe = *loaded_recipe;
            if (!(scene_recipe.material_ssbo_binding = solid.debug_rows[0]).IsValid())
                return false;
            default_recipe = &scene_recipe;
        }

        return LoadStaticMeshSceneAsPrimitiveAssets(
            device,
            geo_mgr,
            solid.geometry_vertex_format,
            default_recipe,
            pack_path,
            base_dir,
            scene_assets_,
            scene_nodes_,
            scene_root_nodes_,
            recipe_table,
            recipe_table_count);
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if (scene_nodes_.empty())
            return false;

        // 纯 TRS 直通：节点局部 TRS 取自 .scene 的 TRSTable 真源，父子层级按
        // parentIndex 用 SetParent 还原 —— 不再对 worldMatrix 做 DecomposeTransform
        // 摊平（旧写法在非均匀缩放嵌套下有旋转/缩放残差，且丢失场景树）。
        StaticMeshSceneInstance::Options options;
        options.mobility = hgl::ecs::Mobility::Static;
        if (pbr_mode_)
        {
            // 行序即 glTF 材质下标 ⇒ 按 material_index 选行，不再轮转
            options.material_rows          = solid.pbr_rows;
            options.material_row_count     = static_cast<uint32_t>(PBR_MATERIAL_COUNT);
            options.rows_by_material_index = true;
        }
        else
        {
            options.material_rows      = solid.debug_rows;
            options.material_row_count = static_cast<uint32_t>(COLOR_COUNT);
        }

        if (!scene_instance_.Instantiate(ecs_context, scene_assets_, scene_nodes_,
                                         scene_root_nodes_, options))
            return false;

        // 实例已自持资产副本 ⇒ 装载缓冲即刻释放，杜绝误用与驻留
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
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

        if (const char * v = getenv("LS_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("LS_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("LS_PITCH")) camera->pitch    = (float)atof(v);

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
    ~LoadSceneApp()
    {
        scene_instance_.Release();
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
    }

    bool InitCull()
    {
        // GPU 剔除 opt-in（默认全关 = 纯 CPU 账基线）
        auto gpu_cull = ecs_context ? ecs_context->GetSystem<hgl::ecs::GPUCullSystem>() : nullptr;
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        return true;
    }

    // PBR 模式的环境光照：主光方向 + 天光 IBL（复用 IBLEnvironment 的烘焙贴图与天空配方）
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

        // 太阳方向必须在天空托管**之前**写进光源行 0：天光调制按当时太阳现算
        light_system->SetMainLightDirection(math::Vector3f(0.2f, 0.7f, 0.68f));

        hgl::ecs::SkyEnvironmentConfig config;
        config.mode         = hgl::ecs::SkyMode::CubeMapIBL;
        config.sky_recipe   = "IBLEnvironment.Sky";
        config.sky_scale    = 1.0f;             // 全天球：包住场景即可
        config.track_camera = true;             // 天空实体逐帧跟随主相机（无视差）

        config.irradiance_texture = IBL_TEX_IRR_CUBE;
        config.prefilter_texture  = IBL_TEX_PRE_CUBE;
        // provider 留空 ⇒ 按 prefilter 后缀自动选择（.TexCube → ambient_light_ibl）

        // 这批烘焙图是 LDR 量化版：显式开启曝光调制（与 IBLEnvironment 同口径）
        config.ldr               = hgl::ecs::SkyAmbientLDR::On;
        config.ldr_exposure_base = 2.5f;
        config.ldr_sun_scale     = 1.0f;
        config.ldr_min_ambient   = 0.05f;

        return environment_system->SetupSkyEnvironment(config);
    }

    bool Init() override
    {
        // 默认走 PBR 材质路径；LS_DEBUGNORMAL=1 回到旧的调试法线/颜色行路径
        pbr_mode_ = (getenv("LS_DEBUGNORMAL") == nullptr);

        if(!InitSolidMDP())
            return(false);

        if(!TryLoadStaticMeshScene())
            return(false);

        if(!InitECS())
            return(false);

        if(pbr_mode_ && !InitEnvironmentControl())
            return(false);

        if(!InitCull())
            return(false);

        if (const char * tag = getenv("LS_DUMP"))
            dump_tag = tag;

        if (const char * v = getenv("LS_FRAMES"))
            ls_frames = (uint32_t)atoi(v);

        return(true);
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // LS_DUMP=<tag>：第 8 帧装一次性帧内回读钩子（本帧提交后、present 前执行 ⇒ 读到本帧画面），
        // 落原始 packed 字节后退出 —— 像素判据用。
        // LSSELFCHECK=1：**帧摘要自检**（A6-d 判据重建；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：PASS 退出 0 / FAIL 退出 1。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        {
            static const bool s_selfcheck = (getenv("LS_SELFCHECK") != nullptr);
            static uint32_t   s_self_frame = 0;

            static constexpr uint32_t kSelfW           = 1280;
            static constexpr uint32_t kSelfH           = 720;
            static constexpr uint32_t kSelfMinDistinct = 2;   // 不空转守卫（恒定图会 FAIL）
            static constexpr uint64_t kSelfFNVDebug    = 0xc9e38ed2f49a069bull;  // 调试法线路径（LS_DEBUGNORMAL=1）
            static constexpr uint64_t kSelfFNVPBR      = 0x407791E46B561248ull;  // PBR 路径（待实跑记录）
                                                        // （改渲染结果必须复核并重录）

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_context ? ecs_context->GetRenderTarget() : nullptr);
                if (!rt)
                {
                    printf("[LSSELF] no swapchain RT\n");
                    std::exit(2);
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                expect.fnv1a64              = pbr_mode_ ? kSelfFNVPBR : kSelfFNVDebug;

                ulre_example::InstallFrameDigestHook(rt, expect, "LSSELF");
            }
        }


        if (!dump_tag.empty() && ++dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_context ? ecs_context->GetRenderTarget() : nullptr);
            if (!rt)
            {
                printf("[LSDUMP] no swapchain RT\n");
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
                    printf("[LSDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "lsdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[LSDUMP] open %s failed\n", name);
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[LSDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        if (ls_frames > 0 && ++ls_frame_seen >= ls_frames)
        {
            printf("[LoadScene] done frames=%u\n", ls_frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};//class LoadSceneApp

int os_main(int argc,os_char **argv)
{
    return RunFramework<LoadSceneApp>(OS_TEXT("Load Scene"),argc,argv,1280,720);
}
