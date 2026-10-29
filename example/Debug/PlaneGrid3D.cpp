// PlaneGrid3D

#include<hgl/framework/WorkManager.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ShaderBufferSources.h>

#include<hgl/color/Color.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/RenderSceneUBOSystem.h>
#include<hgl/ecs/support/GPUCullSystem.h>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include<hgl/vk/VKTextureReadback.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（A6-d 判据重建共用件）

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cstdio>
#include<cstdlib>
#include<string>
#include<vector>
#include<cstring>

using namespace hgl;
using namespace hgl::graph;

class PlaneGrid3DApp:public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;
    hgl::ecs::Entity *camera_entity = nullptr;
    using MaterialDataAccessor =
        graph::GlobalSSBODataAccessor;
    MaterialDataAccessor material_data_accessors[3]{};

    Geometry *         geom_plane_grid     =nullptr;
    graph::mtl::MaterialRecipe plane_grid_recipe{};
    PrimitiveAsset             plane_grid_asset{};

    // ── GPU 剔除台架（A6：**内容线**能否进接管）───────────────────────────
    //   3 张 32×32（sub 8×8）网格 = 数千条线段 ⇒ X 必须按**段数**算（÷2），
    //   公式错则整块网格缺线/错线 ⇒ 像素判据可直接抓。
    //   PG_FRAMES=N    跑 N 帧后退出（CI / 像素对照）
    //   PG_DUMP=<tag>  第 8 帧帧内回读落原始字节后退出（pgdump_<tag>_WxH_fmt.raw）
    std::string pg_dump_tag;
    uint32_t    pg_run_frames = 0;
    uint32_t    pg_dump_frame = 0;
    uint32_t    pg_frame_seen = 0;

    bool InitCull()
    {
        // A6-d T2：**接管已强制**（opt-in 面整体删除）——主帧 mesh 批恒走 GPU 路径。

        auto   gpu_cull = ecs_context->GetSystem<hgl::ecs::GPUCullSystem>();   // shared_ptr（非裸指针）
        if (!gpu_cull)
        {
            printf("[ERROR] InitCull: GPUCullSystem 未注册\n");
            return false;
        }

        printf("[PlaneGrid3D] 台架：接管已强制（A6-d T2）（内容线：3×(32×32, sub 8×8) 网格）\n");
        return true;
    }

    bool CreateRenderObject()
    {
        auto* device = GetDevice();
        auto* geometry_manager = GetManager<GeometryManager>();
        if (!device || !geometry_manager)
            return false;

        using namespace inline_geometry;

        struct PlaneGridCreateInfo pgci;

        pgci.grid_size.Set(32,32);
        pgci.sub_count.Set(8,8);

        pgci.lum=180;
        pgci.sub_lum=255;

        GeometryVertexFormat plane_grid_gvf{
            {VertexSemantic::Position,  VF_V2F},
            {VertexSemantic::Luminance, VF_V1UN8},
        };

        auto pc = std::make_unique<GeometryCreater>(
            device,
            plane_grid_gvf);

        geom_plane_grid=CreatePlaneGrid2D(pc.get(),&pgci);
        if (geom_plane_grid)
            geometry_manager->Add(geom_plane_grid);

        return geom_plane_grid;
    }

    bool Add(
        const char *name,
        const graph::GlobalSSBOBinding &material_ssbo_binding,
        const glm::quat &rotation)
    {
        if (!material_ssbo_binding.IsValid())
            return false;

        auto entity = ecs_context->CreateEntity<hgl::ecs::Entity>(name);
        auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), hgl::ecs::Mobility::Movable));
        auto prim_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
        hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

        transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform.SetLocalRotation(rotation);
        transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform.SetMobility(hgl::ecs::Mobility::Static);

        prim_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(prim_comp->GetOwnerID())->SetPrimitiveAsset(&plane_grid_asset);
        hgl::ecs::MaterialData::MaterialDataAuthoringResource named_struct{};
        named_struct = material_ssbo_binding;
        material_data_comp->SetDataResource(named_struct);
        // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

        return true;
    }

    bool InitScene()
    {
        if(!ecs_context)
            return false;

        const mtl::MaterialRecipe *loaded_recipe =
            GetRecipe("PlaneGrid3D.VertexLuminance");
        if (!loaded_recipe)
            return false;
        plane_grid_recipe = *loaded_recipe;
        if (!(plane_grid_recipe.material_ssbo_binding = material_data_accessors[0].GetGlobalSSBOBinding()).IsValid())
            return false;
        plane_grid_asset = PrimitiveAsset(geom_plane_grid, &plane_grid_recipe, PrimitiveType::Lines);

        if(!Add(
                "PlaneXY",
                material_data_accessors[0].GetGlobalSSBOBinding(),
                glm::quat(1.0f, 0.0f, 0.0f, 0.0f)))
            return false;

        const float rot90 = glm::radians(90.0f);
        if(!Add(
                "PlaneYZ",
                material_data_accessors[1].GetGlobalSSBOBinding(),
                glm::angleAxis(rot90, glm::vec3(0.0f, 1.0f, 0.0f))))
            return false;
        if(!Add(
                "PlaneXZ",
                material_data_accessors[2].GetGlobalSSBOBinding(),
                glm::angleAxis(rot90, glm::vec3(1.0f, 0.0f, 0.0f))))
            return false;

        return true;
    }

    bool InitMISSBO()
    {
        if (!ecs_context)
            return false;

        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        Color4f grid_color = GetColor4f(COLOR::BlenderAxisRed, 1.0f);
        for (uint32_t i = 0; i < 3; ++i)
        {
            material_data_accessors[i] =
                domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
            if (!material_data_accessors[i])
                return false;

            graph::ssbo::EmissiveSurfaceRow row{};
            row.color = grid_color;
            if (!material_data_accessors[i].Write(row))
                return false;

            grid_color = GetColor4f(COLOR(int(COLOR::BlenderAxisRed) + int(i) + 1), 1.0f);
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
        camera->distance = 48.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;

        // 相机 env 门（与其它台架同形：AM_/CV_/PBR_/LS_ 都是这一套 ⇒ 统一情况、可做敏感性对照）。
        // 默认值不动 ⇒ 默认视角不变。原示例只演示「2D 网格转 3D 显示」，无相机控制的特例一并收掉。
        if (const char * v = getenv("PG_YAW"))   camera->yaw      = (float)atof(v);
        if (const char * v = getenv("PG_PITCH")) camera->pitch    = (float)atof(v);
        if (const char * v = getenv("PG_DIST"))  camera->distance = (float)atof(v);
        if (const char * v = getenv("PG_TX"))    camera->target.x = (float)atof(v);
        if (const char * v = getenv("PG_TY"))    camera->target.y = (float)atof(v);
        if (const char * v = getenv("PG_TZ"))    camera->target.z = (float)atof(v);

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

        if(!InitMISSBO())
            return false;

        if(!InitScene())
            return false;

        if(!InitCamera())
            return false;

        return true;
    }

public:
    bool Init() override
    {
        if(!CreateRenderObject())
            return(false);

        if(!InitECS())
            return(false);

        if(!InitCull())
            return(false);

        if (const char * tag = getenv("PG_DUMP"))   pg_dump_tag   = tag;
        if (const char * v   = getenv("PG_FRAMES")) pg_run_frames = (uint32_t)atoi(v);

        return(true);
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // PG_DUMP=<tag>：第 8 帧装一次性帧内回读钩子（本帧提交后、present 前执行 ⇒ 读到
        // 本帧画面），落原始 packed 字节后退出（同 GPUDrivenCull / CoverageVariants 先例）。
        // PGSELFCHECK=1：**帧摘要自检**（A6-d 判据重建；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, FNV-1a 64} 比对下方内联常量：PASS 退出 0 / FAIL 退出 1。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        {
            static const bool s_selfcheck = (getenv("PG_SELFCHECK") != nullptr);
            static uint32_t   s_self_frame = 0;

            static constexpr uint32_t kSelfW           = 1280;
            static constexpr uint32_t kSelfH           = 720;
            static constexpr uint32_t kSelfMinDistinct = 2;   // 不空转守卫（恒定图会 FAIL）
            static constexpr uint64_t kSelfFNV         = 0x4125c3c830cb92d3ull;  // 冻结相机 + 接管下实跑记录
                                                        // （改渲染结果必须复核并重录）

            if (s_selfcheck && ++s_self_frame == 8)
            {
                auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
                if (!rt)
                {
                    printf("[PGSELF] no swapchain RT\n");
                    std::exit(2);
                }

                ulre_example::FrameDigest expect;
                expect.width                = kSelfW;
                expect.height               = kSelfH;
                expect.distinct_byte_values = kSelfMinDistinct;
                expect.fnv1a64              = kSelfFNV;

                ulre_example::InstallFrameDigestHook(rt, expect, "PGSELF");
            }
        }


        if (!pg_dump_tag.empty() && ++pg_dump_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>(ecs_context->GetRenderTarget());
            if (!rt)
            {
                printf("[PGDUMP] no swapchain RT\n");
                std::exit(2);
            }

            std::string tag = pg_dump_tag;
            graph::IRenderTarget * rt_ptr = rt;
            rt->SetInFrameReadbackHook([tag, rt_ptr]()
            {
                std::vector<uint8_t> pixels;
                graph::TextureReadbackInfo info;
                if (!graph::ReadbackColorTarget(rt_ptr, pixels, 0, &info) || pixels.empty())
                {
                    printf("[PGDUMP] readback failed\n");
                    std::exit(2);
                }

                char name[256];
                snprintf(name, sizeof(name), "pgdump_%s_%ux%u_%s.raw",
                         tag.c_str(), info.width, info.height,
                         GetVulkanFormat(info.format) ? GetVulkanFormat(info.format)->name : "unknown");

                FILE * fp = fopen(name, "wb");
                if (!fp)
                {
                    printf("[PGDUMP] open %s failed\n", name);
                    std::exit(2);
                }

                fwrite(pixels.data(), 1, pixels.size(), fp);
                fclose(fp);
                printf("[PGDUMP] wrote %s (%zu bytes)\n", name, pixels.size());
                std::exit(0);
            });
        }

        // PG_FRAMES=N：定帧退出（CI / 像素对照）
        if (pg_run_frames > 0 && ++pg_frame_seen >= pg_run_frames)
        {
            printf("[PG] done frames=%u\n", pg_frame_seen);
            fflush(stdout);
            std::exit(0);
        }
    }
};//class PlaneGrid3DApp:public CameraAppFramework

int os_main(int argc,os_char **argv)
{
    return RunFramework<PlaneGrid3DApp>(OS_TEXT("PlaneGrid3D"),argc,argv,1280,720);
}
