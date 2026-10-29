#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/mesh/StaticMesh.h>
#include<hgl/graph/mesh/LoadStaticMesh.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/color/Color.h>
#include<hgl/type/StdString.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）
#include<hgl/ecs/support/GPUCullSystem.h>
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
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

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

constexpr const COLOR TestColor[] =
{
    COLOR::MozillaCharcoal,
    COLOR::MozillaSand,

    COLOR::BlenderAxisRed,
    COLOR::BlenderAxisGreen,
    COLOR::BlenderAxisBlue,

    COLOR::BananaYellow,
    COLOR::CherryBlossomPink,

    COLOR::SkyBlue,
};

constexpr const size_t COLOR_COUNT = sizeof(TestColor) / sizeof(COLOR);

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
        using MaterialDataAccessor =
            graph::GlobalSSBODataAccessor;

        GeometryVertexFormat geometry_vertex_format;
        MaterialDataAccessor mtl_data_ssbo_accessors[COLOR_COUNT]{};
    };

    MaterialData solid;
    graph::mtl::MaterialRecipe scene_recipe{};

    struct SceneEntity
    {
        hgl::ecs::Entity *entity = nullptr;
        hgl::ecs::TransformAccessor transform;
        hgl::ecs::GeometryData *primitive_comp = nullptr;
    };

    std::vector<PrimitiveAsset>    scene_assets_;
    std::vector<StaticMeshNode>    scene_nodes_;
    std::vector<int32_t>           scene_root_nodes_;
    std::vector<SceneEntity>       scene_entities_;

private:

    bool InitMaterialForDBS(MaterialData *md)
    {
        if (!md)
            return false;

        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        const uint32_t color_count = static_cast<uint32_t>(COLOR_COUNT);
        for (uint32_t i = 0; i < color_count; ++i)
        {
            md->mtl_data_ssbo_accessors[i] =
                domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
            if (!md->mtl_data_ssbo_accessors[i])
                return false;

            graph::ssbo::EmissiveSurfaceRow material_data{};
            material_data.color = GetColor4f(TestColor[i], 1.0f);
            if (!md->mtl_data_ssbo_accessors[i].Write(material_data))
                return false;
        }

        return true;
    }

    bool InitSolidMDP()
    {
        solid.geometry_vertex_format = CreateStandardGeometryVertexFormat(VF_V2UN8);
        if (solid.geometry_vertex_format.GetCount() == 0)
            return false;

        return InitMaterialForDBS(&solid);
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

        const mtl::MaterialRecipe *loaded_recipe =
            GetRecipe("LoadScene.DebugNormalColor");
        if (!loaded_recipe)
            return false;
        scene_recipe = *loaded_recipe;
        if (!(scene_recipe.material_ssbo_binding = solid.mtl_data_ssbo_accessors[0].GetGlobalSSBOBinding()).IsValid())
            return false;

        return LoadStaticMeshSceneAsPrimitiveAssets(
            device,
            geo_mgr,
            solid.geometry_vertex_format,
            &scene_recipe,
            pack_path,
            base_dir,
            scene_assets_,
            scene_nodes_,
            scene_root_nodes_);
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        if (scene_nodes_.empty())
            return false;

        size_t entity_idx = 0;

        for (const auto &node : scene_nodes_)
        {
            for (int32_t pi : node.primitiveIndices)
            {
                if (pi < 0 || pi >= static_cast<int32_t>(scene_assets_.size()))
                    continue;
                PrimitiveAsset &asset = scene_assets_[pi];
                if (!asset.IsValid())
                    continue;

                SceneEntity se;
                se.entity       = ecs_context->CreateEntity<hgl::ecs::Entity>("SceneNode_" + std::to_string(entity_idx++));
                se.transform    = ecs_context->GetTransform(ecs_context->CreateTransform(se.entity->GetEntityID(), hgl::ecs::Mobility::Movable));
                se.primitive_comp = se.entity->GetContext()->GetOrCreateGeometryData(se.entity);
                hgl::ecs::MaterialData *material_data_comp = se.entity->GetContext()->GetOrCreateMaterialData(se.entity);

                // Use pre-computed world matrix for all nodes so child nodes
                // (e.g. Pawn_Top inside Pawn_Body) get the full composed transform.
                //
                // 世界矩阵 → TRS 必须走 math::DecomposeTransform：旧写法
                // quat_cast(glm::mat3(worldMatrix)) 在存在非均匀缩放时输入不是正交基
                // （实测 S=(2,0.5,1)：|q|=1.0768、旋转误差 10.56°、重建误差 0.447），
                // 明细见 src/ecs/tests/ProbeTransformDiagnostics.cpp 的 [T4] 段。
                math::Vector3f world_pos;
                math::Quatf    world_rot;
                math::Vector3f world_scale;
                math::DecomposeTransform(node.worldMatrix,world_pos,world_rot,world_scale);

                se.transform.SetLocalPosition(world_pos);
                se.transform.SetLocalRotation(world_rot);
                se.transform.SetLocalScale(world_scale);
                se.transform.SetMobility(hgl::ecs::Mobility::Static);

                se.primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(se.primitive_comp->GetOwnerID())->SetPrimitiveAsset(&asset);
                hgl::ecs::MaterialData::MaterialDataAuthoringResource scene_struct{};
                scene_struct =
                    solid.mtl_data_ssbo_accessors[(entity_idx - 1) % COLOR_COUNT].GetGlobalSSBOBinding();
                material_data_comp->SetDataResource(scene_struct);
                // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

                scene_entities_.push_back(std::move(se));
            }
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
        scene_entities_.clear();
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

    bool Init() override
    {
        if(!InitSolidMDP())
            return(false);

        if(!TryLoadStaticMeshScene())
            return(false);

        if(!InitECS())
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
            static constexpr uint64_t kSelfFNV         = 0xc9e38ed2f49a069bull;  // 冻结相机 + 接管下实跑记录
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
                expect.fnv1a64              = kSelfFNV;

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
