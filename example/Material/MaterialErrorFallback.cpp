// MaterialErrorFallback
//
// 演示「材质配置写错 ⇒ 材质系统回退到错误棋盘格」：**不同错误种类用不同标记色**，
// 屏幕上肉眼就能分辨是哪一类配置错了。
//
// 场景（引擎世界是 Z-up；6 个球体按 2 行 3 列铺开，相机从 -X 侧看）：
//   #0 对照     引用 builtin/checkerboard_3d      正常                → 灰度棋盘格
//   #1 空定义 ID  mtl_def_id = ""                  MissingMaterialSource     → 洋红棋盘格
//   #2 无材质来源 不设 asset 配方                    MissingMaterialSource     → 洋红棋盘格
//   #3 未知 ID   mtl_def_id = "NotExist/Material"  UnknownMaterialDefinition → 橙棋盘格
//   #4 拼错 ID   mtl_def_id = "builtin/checkerbord_3d"（少一个 'a'）
//                                                  UnknownMaterialDefinition → 橙棋盘格
//   #5 大小写不符 mtl_def_id = "lit"（真实定义是 "Lit"）
//                                                  UnknownMaterialDefinition → 橙棋盘格
//
// 回退机制与本表的定义中枢见 `inc/hgl/mtl/MaterialRecipe.h`：
//   · `kFallbackMaterialRules`      —— 错误种类 → 保底材质定义 ID + 标记色 + 稳定名
//   · `kMarkerlessFallbackMaterialDefinitionID` —— 标记色行拿不到时的零依赖降级
// 运行期把标记色挂到配方上的唯一入口是
// `RenderPrimitiveCollectSystem::AttachFallbackMarkerColor`（走与 builtin/pure_color
// 完全相同的 `EmissiveSurface.color` → `recipe.material_ssbo_binding` 路径）。
//
// 期望日志（每个错误实体一条）：
//   [MaterialFallback] 无材质来源（空 ID）: "" => 回退错误材质 builtin/error_checkerboard_3d
//                      （棋盘格；错误种类 missing_material_source，颜色=(1.00,0.00,1.00,1.00)）
//   [MaterialFallback] 材质定义 ID 无法解析: "NotExist/Material" => 回退错误材质 ...
//                      （棋盘格；错误种类 unknown_material_definition，颜色=(1.00,0.50,0.00,1.00)）
//
// 环境变量：
//   EF_FRAMES=N   跑 N 帧后退出（CI / 抓日志用）
//
// 注意：必须从仓库根目录启动（否则 ShaderLibrary 定位失败，材质库为空 ⇒ 所有材质
// 都落到 UnknownMaterialDefinition，连对照实体也会变橙）。

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/color/Color.h>
#include<hgl/log/Log.h>
#include<hgl/time/Time.h>

// ECS headers
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<cstdlib>     // getenv / atoi / std::exit（示例的定帧退出开关）

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr int   kCaseCount    = 6;
    constexpr float kSphereRadius = 1.0f;
    constexpr float kSpacing      = 3.0f;
    constexpr int   kCaseColumns  = 3;
    constexpr int   kCaseRows     = 2;
    static_assert(kCaseColumns > 0 && kCaseRows > 0, "错误材质示例必须是网格布局");
    static_assert(kCaseColumns * kCaseRows == kCaseCount, "列 × 行必须等于示例数量");

    /// 每个演示例子的说明（只用于日志对照表）。
    struct ErrorCaseDesc
    {
        const char             *entity_name;
        const char             *recipe_name;
        const char             *mtl_def_id;         ///< nullptr = 不设 asset 配方（无材质来源）
        const char             *expected_kind;
        const char             *expected_color;
        const char             *why;
    };

    constexpr ErrorCaseDesc kErrorCases[kCaseCount] =
    {
        { "00_Control",     "EF.Control",  "builtin/checkerboard_3d",
          "None",                     "灰度(0.00/0.35)",
          "对照组：正常引用零依赖棋盘格（不读标记色）" },

        { "01_EmptyID",     "EF.EmptyID",  "",
          "MissingMaterialSource",    "洋红(1.00,0.00,1.00)",
          "配方里 mtl_def_id 留空 ⇒ 无材质来源" },

        { "02_NoSource",    "EF.NoSource", nullptr,
          "MissingMaterialSource",    "洋红(1.00,0.00,1.00)",
          "asset 的 PrimitiveVariant 为空 ⇒ 数据层与 asset 都没有配方" },

        { "03_UnknownID",   "EF.UnknownID", "NotExist/Material",
          "UnknownMaterialDefinition", "橙(1.00,0.50,0.00)",
          "材质定义 ID 在材质库里不存在" },

        { "04_TypoID",      "EF.TypoID",   "builtin/checkerbord_3d",
          "UnknownMaterialDefinition", "橙(1.00,0.50,0.00)",
          "builtin ID 拼写错（少一个 'a'）—— 最容易犯的错" },

        { "05_CaseMismatch", "EF.CaseMismatch", "lit",
          "UnknownMaterialDefinition", "橙(1.00,0.50,0.00)",
          "大小写不符：真实定义是 \"Lit\"（ID 区分大小写）" },
    };

    GeometryVertexFormat CreatePositionNormalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            { VertexSemantic::Position, VF_V3F },
            { VertexSemantic::Normal,   VF_V3F },
        };
        return gvf;
    }
}//namespace

class MaterialErrorFallbackApp : public WorkObject
{
private:

    hgl::ecs::ECSContext *ecs_context = nullptr;

    Geometry *     geoms[kCaseCount]   = {};
    graph::mtl::MaterialRecipe recipes[kCaseCount] = {};
    PrimitiveAsset assets[kCaseCount]  = {};

    uint32_t exit_frames = 0;
    uint32_t frame_seen  = 0;

private:

    bool CreateGeometries()
    {
        auto * device           = GetDevice();
        auto * geometry_manager = GetManager<GeometryManager>();

        if (!device || !geometry_manager)
        {
            GLogError(u8"[MaterialErrorFallback] 没有 Vulkan 设备或几何管理器");
            return false;
        }

        GeometryCreater geometry_creater(device,
                                        CreatePositionNormalGeometryVertexFormat());

        for (int i = 0; i < kCaseCount; ++i)
        {
            geoms[i] = inline_geometry::CreateSphere(&geometry_creater, 24);

            if (!geoms[i])
            {
                GLogError(u8"[MaterialErrorFallback] 创建球体失败：index=%d", i);
                return false;
            }

            geometry_manager->Add(geoms[i]);
        }

        return true;
    }

    /// 组装第 index 个例子的配方 + asset（`mtl_def_id == nullptr` 时**不设**配方）。
    void BuildCaseAsset(const int index)
    {
        const ErrorCaseDesc & desc = kErrorCases[index];

        if (!desc.mtl_def_id)
        {
            // 无材质来源：变体清单为空 ⇒ GeometryData 的 asset 默认配方为 nullptr，
            // 数据层也没有覆盖 ⇒ 分类为 MissingMaterialSource。
            assets[index] = PrimitiveAsset(geoms[index], nullptr, PrimitiveType::Triangles);
            return;
        }

        graph::mtl::MaterialRecipe & recipe = recipes[index];

        recipe.recipe_name = desc.recipe_name;
        recipe.mtl_def_id  = desc.mtl_def_id;
        recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        assets[index] = PrimitiveAsset(geoms[index], &recipe, PrimitiveType::Triangles);
    }

    bool InitScene()
    {
        if (!ecs_context)
            return false;

        for (int i = 0; i < kCaseCount; ++i)
        {
            BuildCaseAsset(i);

            hgl::ecs::Entity * entity =
                ecs_context->CreateEntity<hgl::ecs::Entity>(kErrorCases[i].entity_name);

            if (!entity)
            {
                GLogError(u8"[MaterialErrorFallback] 创建实体失败：index=%d", i);
                return false;
            }

            auto transform = ecs_context->GetTransform(
                ecs_context->CreateTransform(entity->GetEntityID(),
                                             hgl::ecs::Mobility::Static));

            // Z-up：2 行 3 列铺开（列沿 +Y、行沿 +Z），保证一屏内全部可见。
            const int column = i % kCaseColumns;
            const int row    = i / kCaseColumns;

            transform.SetLocalPosition(
                glm::vec3(0.0f,
                          (float(column) - float(kCaseColumns - 1) * 0.5f) * kSpacing,
                          kSphereRadius + kSpacing * 0.5f
                              + (float(kCaseRows - 1) * 0.5f - float(row)) * kSpacing));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(hgl::ecs::Mobility::Static);

            auto * geometry_data =
                entity->GetContext()->GetOrCreateGeometryData(entity->GetEntityID());

            if (!geometry_data)
            {
                GLogError(u8"[MaterialErrorFallback] 取几何组件失败：index=%d", i);
                return false;
            }

            geometry_data->SetPrimitiveAsset(&assets[i]);
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        hgl::ecs::Entity * camera_entity =
            ecs_context->CreateEntity<hgl::ecs::Entity>("MainCamera");

        auto * camera = ecs_context->GetOrCreateCamera(camera_entity);

        if (!camera)
            return false;

        camera->control_mode   = hgl::ecs::CameraControlMode::ViewModel;
        camera->target         = math::Vector3f(0.0f, 0.0f, kSphereRadius + kSpacing * 0.5f);
        camera->distance       = 14.0f;
        camera->yaw            = 0.0f;
        camera->pitch          = -6.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

    void LogCaseTable() const
    {
        GLogInfo(u8"[MaterialErrorFallback] === 错误材质回退演示 ===");
        GLogInfo(u8"[MaterialErrorFallback] 共 %d 个球体，2 行 3 列（Z-up 世界，列沿 +Y、行沿 +Z）", kCaseCount);

        for (int i = 0; i < kCaseCount; ++i)
        {
            const ErrorCaseDesc & desc = kErrorCases[i];

            GLogInfo(u8"[MaterialErrorFallback]   #%d %-16s mtl_def_id=%-28s 期望种类=%-26s 期望颜色=%s  /* %s */",
                     i,
                     desc.entity_name,
                     desc.mtl_def_id ? desc.mtl_def_id : "<无 asset 配方>",
                     desc.expected_kind,
                     desc.expected_color,
                     desc.why);
        }

        GLogInfo(u8"[MaterialErrorFallback] 回退规则表（%u 种错误）见 inc/hgl/mtl/MaterialRecipe.h:"
                 u8"kFallbackMaterialRules", graph::mtl::MATERIAL_ERROR_KIND_COUNT);
    }

public:

    ~MaterialErrorFallbackApp()
    {
        for (int i = 0; i < kCaseCount; ++i)
            SAFE_CLEAR(geoms[i]);
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.06f, 0.06f, 0.07f, 1.0f));

        ecs_context = GetECSContext();

        if (!ecs_context)
        {
            GLogError(u8"[MaterialErrorFallback] 没有 ECSContext");
            return false;
        }

        if (!CreateGeometries()) return false;
        if (!InitScene())        return false;
        if (!InitCamera())       return false;

        if (const char * value = getenv("EF_FRAMES"))
            exit_frames = (uint32_t)atoi(value);

        LogCaseTable();
        return true;
    }

    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        if (exit_frames == 0)
            return;

        if (++frame_seen >= exit_frames)
        {
            GLogInfo(u8"[MaterialErrorFallback] 已跑满 %u 帧，退出", frame_seen);
            std::exit(0);
        }
    }
};//class MaterialErrorFallbackApp

int os_main(int argc, os_char **argv)
{
    return RunFramework<MaterialErrorFallbackApp>(
        OS_TEXT("MaterialErrorFallback"), argc, argv, 1280, 720);
}
