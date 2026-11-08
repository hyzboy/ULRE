// PBRMatrix —— Filament《materials.spheres》口径的纯球 PBR 属性矩阵示例。
//
// 10×10 全部使用同一个内建球（geo:builtin:sphere_64），画面唯一变量是材质行：
//   col → metallic   0.0 → 1.0
//   row → roughness  0.045 → 1.0（Filament MIN_PERCEPTUAL_ROUGHNESS 起步）
// 无任何贴图（配方不带 [[textures]]，数据行底色统一 0.72 灰、fresnel 走缺省 0.04），
// 用来肉眼检视阶段一 BRDF（Correlated GGX / Smith / 解析 DFG / 能量补偿）的正确性。
//
// 自检台架（与 PBRSpheres 同款，共用 example/Shared/FrameDigest.h）：
//   PBRM_SELFCHECK=1  第 8 帧冻结回读 → {尺寸, distinct 下限, FNV-1a64} 比对内联常量；
//   PBRM_FRAMES=N     定帧退出（性能/冒烟）；
//   PBRM_PITCH / PBRM_YAW / PBRM_DIST / PBRM_TX / PBRM_Z  相机覆盖（敏感性对照）。

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
    // 10×10 网格参数（与 PBRSpheres 同口径，方便两例并排对比）
    constexpr uint GRID_SIZE       = 10;    // 每轴球数
    constexpr float SPHERE_SPACING = 2.5f;  // 中心间距

    // 材质数据行唯一来源（col→metallic，row→roughness，块内第 i 行 == 池行号 first+i）
    constexpr const char * MATRIX_DATA_PATH =
        "mtldata:pack(ShaderLibrary):asset/material_data/pbr_matrix_10x10.material_data.toml";

    // 帧摘要判据常量：冻结相机 + 第 8 帧，Debug 下实跑记录。
    // ⚠ 改渲染结果（BRDF/数据行/相机）必须复核并重录（FrameDigest.h 台架规矩）。
    constexpr uint32_t kSelfW           = 1280;
    constexpr uint32_t kSelfH           = 720;
    constexpr uint32_t kSelfMinDistinct = 2;      // 不空转守卫（恒定图会 FAIL）
    constexpr uint64_t kSelfFNV         = 0xB65C1C88F65487DCull;  // 冻结相机 + Debug 实跑记录
                                                                  // （改渲染结果必须复核并重录）
}

class PBRMatrixApp : public WorkObject
{
private:
    ECSContext *  ecs_world     = nullptr;
    Entity *      camera_entity = nullptr;

    bool         selfcheck_enabled = false;
    uint32_t     frame_seen  = 0;
    uint32_t     perf_frames = 0;   ///< PBRM_FRAMES=N：定帧退出；0 = 未启用

    graph::mtl::MaterialRecipe matrix_recipe{};

    VertexDataManager * mesh_vdm  = nullptr;
    Geometry *          sphere_geo = nullptr;
    PrimitiveAsset      base_primitive{};

    graph::mtl::MaterialDataBlockAsset * matrix_data_block = nullptr;

public:
    ~PBRMatrixApp() override
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

private:
    bool InitMaterial()
    {
        const mtl::MaterialRecipe *loaded_recipe = GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Material/PBRMatrix.Lit");
        if (!loaded_recipe)
        {
            GLogError(u8"PBRMatrix: 配方 PBRMatrix.Lit 加载失败");
            return false;
        }
        matrix_recipe = *loaded_recipe;
        return true;
    }

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
        {
            GLogError(u8"PBRMatrix: 无 BufferManager");
            return false;
        }

        // 与 PBRSpheres 同型的标准纹理坐标顶点格式（球几何自带 UV，本示例不采样）
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };

        mesh_vdm = new VertexDataManager(buffer_manager, gvf);
        if (!mesh_vdm)
        {
            GLogError(u8"PBRMatrix: VertexDataManager 创建失败");
            return false;
        }

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
        {
            GLogError(u8"PBRMatrix: VertexDataManager 初始化失败");
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
            GLogError(u8"PBRMatrix: 内建球几何创建失败");
            return false;
        }
        return true;
    }

    bool InitMaterialDataBlock()
    {
        auto *graphics = GetGraphicsContext();
        if (!graphics)
        {
            GLogError(u8"PBRMatrix: 无 GraphicsContext");
            return false;
        }

        auto *blocks = graphics->GetMaterialDataBlockRegistry();
        auto *pool   = GetManager<GlobalSSBOBufferRegistry>();
        if (!blocks || !pool)
        {
            GLogError(u8"PBRMatrix: 无 MaterialDataBlockRegistry/pool");
            return false;
        }

        matrix_data_block = blocks->GetOrLoadURI(ToOSString(MATRIX_DATA_PATH));
        if (!matrix_data_block)
        {
            GLogError(u8"PBRMatrix: 数据块加载失败：%s", MATRIX_DATA_PATH);
            return false;
        }

        if (!matrix_data_block->Upload(pool))
        {
            GLogError(u8"PBRMatrix: 数据块上传失败：%s",
                      matrix_data_block->GetFailureReason().c_str());
            return false;
        }

        if (matrix_data_block->GetRowCount() != GRID_SIZE * GRID_SIZE)
        {
            GLogError(u8"PBRMatrix: 行数 %u != %u",
                      matrix_data_block->GetRowCount(), GRID_SIZE * GRID_SIZE);
            return false;
        }
        return true;
    }

    bool InitECS()
    {
        ecs_world = GetECSContext();
        if (!ecs_world)
        {
            GLogError(u8"PBRMatrix: 无 ECSContext");
            return false;
        }

        if (!InitMaterialDataBlock())
            return false;

        // 配方 material SSBO 绑定 = 块所在池 + 块首行；逐球行号由 per-entity 4-ID 表承载
        const graph::GlobalSSBOBinding block_binding{
            matrix_data_block->GetGlobalSSBOType(),
            matrix_data_block->GetSSBOId(),
            matrix_data_block->RowIDAt(0)};

        if (!(matrix_recipe.material_ssbo_binding = block_binding).IsValid())
        {
            GLogError(u8"PBRMatrix: 材质 SSBO 绑定无效");
            return false;
        }

        base_primitive = PrimitiveAsset(sphere_geo, &matrix_recipe, PrimitiveType::Triangles);

        // 网格原点居中于世界原点；场景静止 ⇒ 全部 Static（零脏行/帧）
        const float offset = (GRID_SIZE - 1) * SPHERE_SPACING * 0.5f;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                const AnsiString name =
                    AnsiString("Sphere_M") + AnsiString::numberOf(col) +
                    AnsiString("_R")       + AnsiString::numberOf(row);

                Entity *e = RenderableBuilder(ecs_world, name.c_str())
                    .WithTransform(glm::vec3(col * SPHERE_SPACING - offset,
                                             row * SPHERE_SPACING - offset,
                                             0.0f),
                                   glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   glm::vec3(1.0f),
                                   Mobility::Static)
                    .WithPrimitiveAsset(&base_primitive)
                    .WithMaterialDataRow(ToOSString(MATRIX_DATA_PATH),
                                         row * GRID_SIZE + col)
                    .Build();
                if (!e)
                {
                    GLogError(u8"PBRMatrix: 实体构建失败 (%u,%u)", row, col);
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
            GLogError(u8"PBRMatrix: CameraSystem 创建失败");
            return false;
        }

        camera_entity = ecs_world->CreateEntity<Entity>("MainCamera");
        auto *camera  = ecs_world->GetOrCreateCamera(camera_entity);

        // 俯视整个 10×10 网格：拉远 + 低俯角，保留掠射角高光带（BRDF 敏感区）
        camera->control_mode = CameraControlMode::ViewModel;
        camera->target       = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance     = 40.0f;
        camera->yaw          = 0.0f;
        camera->pitch        = -25.0f;

        if (const char *v = getenv("PBRM_PITCH"))
            camera->pitch = (float)atof(v);
        if (const char *v = getenv("PBRM_YAW"))
            camera->yaw = (float)atof(v);
        if (const char *v = getenv("PBRM_DIST"))
            camera->distance = (float)atof(v);
        if (const char *v = getenv("PBRM_TX"))
            camera->target.x = (float)atof(v);
        if (const char *v = getenv("PBRM_TZ"))
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

        selfcheck_enabled = (getenv("PBRM_SELFCHECK") != nullptr);
        if (const char *v = getenv("PBRM_FRAMES"))
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
                GLogError(u8"PBRMSELF: 无 swapchain RT");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = kSelfFNV;

            ulre_example::InstallFrameDigestHook(rt, expect, "PBRMSELF");
        }

        if (perf_frames > 0 && frame_seen >= perf_frames)
        {
            GLogInfo(u8"PBRM-PERF: done frames=%u", frame_seen);
            std::exit(0);
        }
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<PBRMatrixApp>(
        OS_TEXT("PBR Matrix 10x10 - Filament materials.spheres"), argc, argv, 1280, 720);
}
