// PBRAdvanced —— Filament 高级材质四件套的纯球演示（ClearCoat / Sheen / 各向异性 / 简易次表面）。
//
// 4×10 全部使用同一个内建球（geo:builtin:sphere_64），每 band 沿 col 扫一个高级参数：
//   band 0（车漆）    clearcoat         0 → 1
//   band 1（丝绸）    sheen_color 亮度  0 → 1
//   band 2（拉丝金属）anisotropy        0 → 1
//   band 3（玉石）    subsurface_power  0 → 1
// 无任何贴图；与 PBRMatrix 同款台架，差别只在域选 StandardAdvanced、
// 数据行是 64B 的 PBRAdvancedSurface（stage 2 的 SSBO 行）。
//
// 自检台架（与 PBRMatrix 同款，共用 example/Shared/FrameDigest.h）：
//   PBRADV_SELFCHECK=1  第 8 帧冻结回读 → {尺寸, distinct 下限, FNV-1a64} 比对内联常量；
//   PBRADV_FRAMES=N     定帧退出（性能/冒烟）；
//   PBRADV_PITCH / PBRADV_YAW / PBRADV_DIST / PBRADV_TX / PBRADV_TZ  相机覆盖（敏感性对照）。

#include<hgl/framework/WorkManager.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/mtl/MaterialResolver.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/asset/MaterialDataBlockAsset.h>

#include<hgl/filesystem/Filename.h>
#include<hgl/type/String.h>
#include<hgl/color/ColorPacking.h>
#include<hgl/log/Log.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/support/pipeline/RenderableBuilder.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检共用件

#include <cstdlib>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 4 band × 10 col 网格参数（与 PBRMatrix 同口径的间距，便于并排对比）
    constexpr uint GRID_COLS   = 10;    // 每 band 球数（参数扫描轴）
    constexpr uint GRID_ROWS   = 4;     // band 数（每行一个高级特性）
    constexpr float SPHERE_SPACING = 2.5f;  // 中心间距

    // 材质数据行唯一来源（块内第 i 行 == 池行号 first+i）
    constexpr const char * ADV_DATA_PATH =
        "mtldata:pack(ShaderLibrary):asset/material_data/pbr_advanced_matrix.material_data.toml";

    // 帧摘要判据常量：冻结相机 + 第 8 帧，Debug 下实跑记录。
    // ⚠ 改渲染结果（BRDF/数据行/相机）必须复核并重录（FrameDigest.h 台架规矩）。
    constexpr uint32_t kSelfW           = 1280;
    constexpr uint32_t kSelfH           = 720;
    constexpr uint32_t kSelfMinDistinct = 2;      // 不空转守卫（恒定图会 FAIL）
    constexpr uint64_t kSelfFNV         = 0x2DA9D1F43EF1481Eull;  // Debug 实跑记录（A2BGR10UN 回读，Filament 纠正后）
}

class PBRAdvancedApp : public WorkObject
{
private:
    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    bool         selfcheck_enabled = false;
    uint32_t     frame_seen  = 0;
    uint32_t     perf_frames = 0;   ///< PBRADV_FRAMES=N：定帧退出；0 = 未启用

    graph::mtl::MaterialRecipe adv_recipe{};

    VertexDataManager * mesh_vdm  = nullptr;
    Geometry *          sphere_geo = nullptr;
    PrimitiveAsset      base_primitive{};

    graph::mtl::MaterialDataBlockAsset * adv_data_block = nullptr;

public:
    ~PBRAdvancedApp() override
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

private:
    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Material/PBRAdvanced.Lit");
        if (!loaded_recipe)
        {
            GLogError(u8"PBRAdvanced: 配方 PBRAdvanced.Lit 加载失败");
            return false;
        }
        adv_recipe = *loaded_recipe;
        return true;
    }

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
        {
            GLogError(u8"PBRAdvanced: 无 BufferManager");
            return false;
        }

        // 与 PBRMatrix 同型的标准纹理坐标顶点格式（球几何自带 UV，本示例不采样）
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };

        mesh_vdm = new VertexDataManager(buffer_manager, gvf);
        if (!mesh_vdm)
        {
            GLogError(u8"PBRAdvanced: VertexDataManager 创建失败");
            return false;
        }

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
        {
            GLogError(u8"PBRAdvanced: VertexDataManager 初始化失败");
            return false;
        }
        return true;
    }

    bool CreateSphereGeometry()
    {
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        sphere_geo = GetGeometry("geo:builtin:sphere_64");
        if (!sphere_geo)
        {
            GLogError(u8"PBRAdvanced: 内建球几何创建失败");
            return false;
        }
        return true;
    }

    bool InitMaterialDataBlock()
    {
        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            GLogError(u8"PBRAdvanced: 无 GraphicsContext");
            return false;
        }

        auto *blocks = graphics->GetMaterialDataBlockRegistry();
        auto *pool   = GetManager<GlobalSSBOBufferRegistry>();
        if (!blocks || !pool)
        {
            GLogError(u8"PBRAdvanced: 无 MaterialDataBlockRegistry/pool");
            return false;
        }

        adv_data_block = blocks->GetOrLoadURI(ToOSString(ADV_DATA_PATH));
        if (!adv_data_block)
        {
            GLogError(u8"PBRAdvanced: 数据块加载失败：%s", ADV_DATA_PATH);
            return false;
        }

        if (!adv_data_block->Upload(pool))
        {
            GLogError(u8"PBRAdvanced: 数据块上传失败：%s",
                      adv_data_block->GetFailureReason().c_str());
            return false;
        }

        if (adv_data_block->GetRowCount() != GRID_COLS * GRID_ROWS)
        {
            GLogError(u8"PBRAdvanced: 行数 %u != %u",
                      adv_data_block->GetRowCount(), GRID_COLS * GRID_ROWS);
            return false;
        }
        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            GLogError(u8"PBRAdvanced: 无 ECSContext");
            return false;
        }

        if (!InitMaterialDataBlock())
            return false;

        // 配方 material SSBO 绑定 = 块所在池 + 块首行；逐球行号由 per-entity 4-ID 表承载
        const graph::GlobalSSBOBinding block_binding{
            adv_data_block->GetGlobalSSBOType(),
            adv_data_block->GetSSBOId(),
            adv_data_block->RowIDAt(0)};

        if (!(adv_recipe.material_ssbo_binding = block_binding).IsValid())
        {
            GLogError(u8"PBRAdvanced: 材质 SSBO 绑定无效");
            return false;
        }

        base_primitive = PrimitiveAsset(sphere_geo, &adv_recipe, PrimitiveType::Triangles);

        // 网格原点居中于世界原点；场景静止 ⇒ 全部 Static（零脏行/帧）
        const float offset_x = (GRID_COLS - 1) * SPHERE_SPACING * 0.5f;
        const float offset_y = (GRID_ROWS - 1) * SPHERE_SPACING * 0.5f;

        for (uint row = 0; row < GRID_ROWS; ++row)
        {
            for (uint col = 0; col < GRID_COLS; ++col)
            {
                const AnsiString name =
                    AnsiString("Sphere_Adv_B") + AnsiString::numberOf(row) +
                    AnsiString("_C")          + AnsiString::numberOf(col);

                Entity *e = RenderableBuilder(ecs_world, name.c_str())
                    .WithTransform(glm::vec3(col * SPHERE_SPACING - offset_x,
                                             row * SPHERE_SPACING - offset_y,
                                             0.0f),
                                   glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   glm::vec3(1.0f),
                                   Mobility::Static)
                    .WithPrimitiveAsset(&base_primitive)
                    .WithMaterialDataRow(ToOSString(ADV_DATA_PATH),
                                         row * GRID_COLS + col)
                    .Build();
                if (!e)
                {
                    GLogError(u8"PBRAdvanced: 实体构建失败 (%u,%u)", row, col);
                    return false;
                }
            }
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_world->EnsureCameraSystem())
        {
            GLogError(u8"PBRAdvanced: CameraSystem 创建失败");
            return false;
        }

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto *camera  = ecs_world->GetOrCreateCamera(camera_entity);

        // 俯视整个 4×10 网格：拉远 + 低俯角，保留掠射角高光带（BRDF 敏感区）
        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance     = 36.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -25.0f;

        if (const char *v = getenv("PBRADV_PITCH"))
            camera->pitch = (float)atof(v);
        if (const char *v = getenv("PBRADV_YAW"))
            camera->yaw = (float)atof(v);
        if (const char *v = getenv("PBRADV_DIST"))
            camera->distance = (float)atof(v);
        if (const char *v = getenv("PBRADV_TX"))
            camera->target.x = (float)atof(v);
        if (const char *v = getenv("PBRADV_TZ"))
            camera->target.z = (float)atof(v);

        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();
        return true;
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        selfcheck_enabled = (getenv("PBRADV_SELFCHECK") != nullptr);
        if (const char *v = getenv("PBRADV_FRAMES"))
            perf_frames = (uint32_t)atoi(v);

        if (!InitMaterial())  return false;
        if (!InitVDM())       return false;
        if (!CreateSphereGeometry()) return false;
        if (!InitECS())       return false;
        if (!InitCamera())    return false;
        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);
        ++frame_seen;

        // 场景静止（无动画）⇒ 任意帧的画面都确定；判据帧沿用第 8 帧口径。
        if (selfcheck_enabled && frame_seen == 8)
        {
            auto *rt = dynamic_cast<graph::SwapchainRenderTarget *>(
                ecs_world ? ecs_world->GetRenderTarget() : nullptr);
            if (!rt)
            {
                GLogError(u8"PBRADVSELF: 无 swapchain RT");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "PBRADVSELF");
        }

        if (perf_frames > 0 && frame_seen >= perf_frames)
        {
            GLogInfo(u8"PBRADV-PERF: done frames=%u", frame_seen);
            std::exit(0);
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<PBRAdvancedApp>(
        OS_TEXT("PBR Advanced 4x10 - Filament ClearCoat/Sheen/Aniso/SubSurface"), argc, argv, 1280, 720);
}
