// GLTF_DamagedHelmet —— khronos glTF-Sample-Models 的 DamagedHelmet 单材质 PBR 演示。
//
// 资产管线：res/model/DamagedHelmet/glTF → GLTFConvert → res/DamagedHelmet.StaticMesh。
// glTF 的 SPLIT 贴图（metalRoughness: G=rough/B=metal + 独立 occlusion）已离线
// 重打包为真 ORM（R=AO/G=rough/B=metal，见 DamagedHelmet.ORM.png → .Tex2D），
// 与 StandardOpaque 的 orm 槽解包口径一致；emissive 纹理是唯一自发光来源
// （PBRSurface 行没有 emissive 因子字段）。
//
// 材质数值唯一来源：DamagedHelmet.material_data.toml（1 行 PBRSurface，
// glTF 缺省因子口径 base_color=白 / metallic=1 / roughness=1）。
//
// 自检台架（与 LoadModel/PBRMatrix 同款，共用 example/Shared/FrameDigest.h）：
//   HDH_SELFCHECK=1  第 8 帧冻结回读 → {尺寸, distinct 下限, FNV-1a64} 比对内联常量；
//   HDH_FRAMES=N     定帧退出（性能/冒烟）；
//   HDH_DIST / HDH_YAW / HDH_PITCH  相机覆盖（敏感性对照）。

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/graph/asset/AssetSourceMount.h>
#include<hgl/graph/asset/StaticMeshModelInstance.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检共用件
#include<hgl/ecs/support/cull/GPUCullSystem.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/light_shadow/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/light_shadow/LightSystem.h>

#include<glm/glm.hpp>

#include<cstdlib>
#include<cstdio>
#include<filesystem>
#include<memory>
#include<string>
#include<vector>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    // 自装配：scene/recipe/material_data/tex2d 全部取自 pack 所在目录，无任何硬编码 URI。
    const std::string &ModelPackPath()
    {
        static const std::string path = []
        {
            const char *scene_env = getenv("HDH_SCENE");
            return std::string(scene_env ? scene_env
                                         : "pack(res):DamagedHelmet.StaticMesh/DamagedHelmet.model");
        }();
        return path;
    }

    // 天光 IBL：与 LoadModel / IBLEnvironment 同一批烘焙贴图与天空配方
    constexpr os_char IBL_TEX_IRR_CUBE[] = OS_TEXT("pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube");
    constexpr os_char IBL_TEX_PRE_CUBE[] = OS_TEXT("pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube");

    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2HF},   // UV RG16F（与棋件场景同口径）
            {VertexSemantic::Normal,   VF_V2UN8},
        };
        return gvf;
    }

    // 帧摘要判据常量：冻结相机 + 定帧，实跑记录。
    // 异步纹理流式加载 4 张 2K 贴图在 20 帧左右全部 Resident，判据帧设为第 40 帧保证模型完整渲染。
    constexpr uint32_t kSelfCheckFrame  = 40;
    constexpr uint32_t kSelfW           = 1280;
    constexpr uint32_t kSelfH           = 720;
    constexpr uint32_t kSelfMinDistinct = 2;      // 不空转守卫（恒定图会 FAIL）
    constexpr uint64_t kSelfFNV         = 0x51D0CD111D41CF1EULL;   // 实跑记录（Debug 锚定）
}

class GLTFDamagedHelmetApp : public WorkObject
{
private:
    hgl::ecs::ECSContext * ecs_context   = nullptr;
    hgl::ecs::Entity *     camera_entity = nullptr;

    bool         selfcheck_enabled = false;
    uint32_t     frame_seen  = 0;
    uint32_t     perf_frames = 0;   ///< HDH_FRAMES=N：定帧退出；0 = 未启用
    std::string  dump_tag;          ///< HDH_DUMP=<tag>：第 8 帧回读落盘后退出

    GeometryVertexFormat     geometry_vertex_format;
    SelfContainedModelMaterials scene_materials_;

    std::shared_ptr<hgl::ecs::EnvironmentSystem> environment_system;
    std::shared_ptr<hgl::ecs::LightSystem>       light_system;

    std::vector<PrimitiveAsset>  scene_assets_;
    std::vector<StaticMeshNode>  scene_nodes_;
    std::vector<int32_t>         scene_root_nodes_;
    StaticMeshModelInstance      model_instance_;

public:
    ~GLTFDamagedHelmetApp() override
    {
        model_instance_.Release();
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
    }

private:
    bool InitMaterial()
    {
        geometry_vertex_format = CreateStandardGeometryVertexFormat();
        if (geometry_vertex_format.GetCount() == 0)
        {
            GLogError(u8"GLTF_DamagedHelmet: 顶点格式创建失败");
            return false;
        }

        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            GLogError(u8"GLTF_DamagedHelmet: 无 GraphicsContext");
            return false;
        }

        // 自装配：材质名来自 pack 的 MaterialTable，配方/数据块来自模型目录
        if (!BuildSelfContainedModelMaterials(
                *graphics, hgl::ToOSString(ModelPackPath()), nullptr, scene_materials_))
        {
            GLogError(u8"GLTF_DamagedHelmet: 自包含材质装配失败");
            return false;
        }
        return true;
    }

    bool TryLoadModel()
    {
        using std::filesystem::exists;
        using std::filesystem::path;

        // 请求串可以是裸路径或包裹 URI：先经挂载索引反查成物理路径，再上盘核验存在性
        hgl::OSString resolved;
        if (!hgl::graph::ResolveAssetPhysicalPath(hgl::ToOSString(ModelPackPath()), resolved) || resolved.IsEmpty())
        {
            AnsiString p = AnsiString(ModelPackPath().c_str());
            GLogError(u8"GLTF_DamagedHelmet: 场景资产不存在：%s", p.c_str());
            return false;
        }

        const path scene_path(hgl::ToStdString(resolved));
        if (!exists(scene_path))
        {
            AnsiString p = AnsiString(scene_path.string().c_str());
            GLogError(u8"GLTF_DamagedHelmet: 场景资产不存在：%s", p.c_str());
            return false;
        }

        auto * device  = GetDevice();
        auto * geo_mgr = GetManager<GeometryManager>();
        if (!device || !geo_mgr)
        {
            GLogError(u8"GLTF_DamagedHelmet: 无 Device/GeometryManager");
            return false;
        }

        const OSString pack_path = hgl::ToOSString(scene_path.string());
        const OSString base_dir  = hgl::ToOSString(scene_path.parent_path().string());

        // 自装配：default 用第一份配方副本，逐材质表覆盖各自绑定
        return LoadModelAsPrimitiveAssets(
            device,
            geo_mgr,
            geometry_vertex_format,
            scene_materials_.fallback,
            pack_path,
            base_dir,
            scene_assets_,
            scene_nodes_,
            scene_root_nodes_,
            scene_materials_.table.data(),
            static_cast<uint32_t>(scene_materials_.table.size()));
    }

    bool InitScene()
    {
        if (!ecs_context || scene_nodes_.empty())
            return false;

        StaticMeshModelInstance::Options options;
        options.mobility         = hgl::ecs::Mobility::Static;
        options.material_rows    = scene_materials_.bindings.data();
        options.material_row_count = static_cast<uint32_t>(scene_materials_.bindings.size());
        options.rows_by_material_index = true;   // 行 i = pack 材质表第 i 行

        if (!model_instance_.Instantiate(ecs_context, scene_assets_, scene_nodes_,
                                         scene_root_nodes_, options))
        {
            GLogError(u8"GLTF_DamagedHelmet: 场景实例化失败");
            return false;
        }

        // 实例已自持资产副本 ⇒ 装载缓冲即刻释放
        scene_assets_.clear();
        scene_nodes_.clear();
        scene_root_nodes_.clear();
        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context->EnsureCameraSystem())
        {
            GLogError(u8"GLTF_DamagedHelmet: CameraSystem 创建失败");
            return false;
        }

        camera_entity = ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");
        auto * camera = ecs_context->GetOrCreateCamera(camera_entity);

        // 头盔半径约 1：近距离略俯角，正面偏侧看破损面
        camera->control_mode = hgl::ecs::CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);
        // 头盔半径约 1.28：机位设为 2.5 距离近景特写，略俯角看破损面
        camera->distance     = 2.5f;
        camera->yaw          = 45.0f;
        camera->pitch        = -20.0f;

        if (const char * v = getenv("HDH_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("HDH_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("HDH_PITCH")) camera->pitch    = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();
        return true;
    }

    // 主光方向 + 天光 IBL（与 LoadModel PBR 模式同配置口径）
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
        config.mode              = hgl::ecs::SkyMode::CubeMapIBL;
        config.sky_recipe        = "IBLEnvironment.Sky";
        config.sky_scale         = 1.0f;
        config.track_camera      = true;
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

    bool InitCull()
    {
        auto gpu_cull = ecs_context ? ecs_context->GetSystem<hgl::ecs::GPUCullSystem>() : nullptr;
        if (!gpu_cull)
        {
            GLogError(u8"GLTF_DamagedHelmet: GPUCullSystem 未注册");
            return false;
        }
        return true;
    }

    bool InitECS()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
        {
            GLogError(u8"GLTF_DamagedHelmet: 无 ECSContext");
            return false;
        }

        if (!InitScene())  return false;
        if (!InitCamera()) return false;
        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        selfcheck_enabled = (getenv("HDH_SELFCHECK") != nullptr);
        if (const char * v = getenv("HDH_FRAMES"))
            perf_frames = (uint32_t)atoi(v);
        if (const char * tag = getenv("HDH_DUMP"))
            dump_tag = tag;

        if (!InitMaterial())        return false;
        if (!TryLoadModel()) return false;

        if (!InitECS())             return false;
        if (!InitEnvironmentControl()) return false;
        if (!InitCull())            return false;
        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
        ++frame_seen;

        // 场景静止（无动画）⇒ 任意帧画面都确定；判据帧设为第 40 帧（纹理完全 Resident）。
        if (selfcheck_enabled && frame_seen == kSelfCheckFrame)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_context ? ecs_context->GetRenderTarget() : nullptr);
            if (!rt)
            {
                GLogError(u8"HDHSELF: 无 swapchain RT");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "HDHSELF");
        }

        if (perf_frames > 0 && frame_seen >= perf_frames)
        {
            GLogInfo(u8"HDH-PERF: done frames=%u", frame_seen);
            std::exit(0);
        }

        // HDH_DUMP=<tag>：帧内回读落原始字节后退出（默认第 8 帧，可通过 HDH_DUMP_FRAME 覆盖）
        uint32_t target_dump_frame = 8;
        if (const char * df = getenv("HDH_DUMP_FRAME"))
            target_dump_frame = (uint32_t)atoi(df);

        if (!dump_tag.empty() && frame_seen == target_dump_frame)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_context ? ecs_context->GetRenderTarget() : nullptr);
            if (!rt)
            {
                GLogError(u8"HDHDUMP: 无 swapchain RT");
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
                    GLogError(u8"HDHDUMP: 回读失败");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "hdhdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");
                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    GLogError(u8"HDHDUMP: 打开 %s 失败", AnsiString(name).c_str());
                    std::exit(2);
                }
                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                GLogInfo(u8"HDHDUMP: 写出 %s（%llu 字节）", AnsiString(name).c_str(), (unsigned long long)pixels.size());
                std::exit(0);
            });
        }
    }
};

int os_main(int argc, os_char ** argv)
{
    return RunFramework<GLTFDamagedHelmetApp>(
        OS_TEXT("GLTF DamagedHelmet - PBR ORM + IBL"), argc, argv, 1280, 720);
}
