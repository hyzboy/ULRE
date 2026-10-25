// MaterialErrorFallback
//
// 演示「材质配置写错 ⇒ 材质系统回退到错误棋盘格」：**不同错误种类用不同标记色**，
// 屏幕上肉眼就能分辨是哪一类配置错了。
//
// 场景（引擎世界是 Z-up；9 个球体按 3 行 3 列铺开，相机从 -X 侧看）：
//   #0 对照     引用 builtin/checkerboard_3d      正常                → 灰度棋盘格
//   #1 空定义 ID  mtl_def_id = ""                  MissingMaterialSource     → 洋红棋盘格
//   #2 无材质来源 不设 asset 配方                    MissingMaterialSource     → 洋红棋盘格
//   #3 未知 ID   mtl_def_id = "NotExist/Material"  UnknownMaterialDefinition → 橙棋盘格
//   #4 拼错 ID   mtl_def_id = "builtin/checkerbord_3d"（少一个 'a'）
//                                                  UnknownMaterialDefinition → 橙棋盘格
//   #5 大小写不符 mtl_def_id = "lit"（真实定义是 "Lit"）
//                                                  UnknownMaterialDefinition → 橙棋盘格
//   #6 缺纹理     mtl_def_id = "UnlitTexture"（该定义声明 base_color required=true），
//                 但配方不给任何纹理资源            MissingTextureResource     → 青棋盘格
//   #7 缺顶点法线 mtl_def_id = "Lit"，几何顶点只有 Position+TexCoord（**没有 Normal**）
//                                                  MissingVertexNormal        → 黄棋盘格
//   #8 缺纹理UV   mtl_def_id = "Lit"，几何顶点只有 Position+Normal（**没有 TexCoord**）
//                                                  MissingTextureUV           → 蓝棋盘格
//
// #7/#8 的语义（用户 2026-10-24 两次拍板，后者覆盖前者）：
//   初版口径是「材质要法线光照、顶点没 NTB ⇒ 三角形平面渲染，这是引擎支持的**正常光照
//   模式**，所以只是降级、不算错误」。被推翻 —— 面法线平面着色看起来「像正常的低模」，
//   法线数据坏了这件事就被完全掩掉了；缺 UV 则按垃圾 UV 采样，表现为随机花纹，比没贴图
//   更难查。**两类都必须肉眼一眼可见** ⇒ 一律回退错误棋盘格，只用颜色区分种类。
//
//   这两类与 #1~#6 的区别：**材质定义与配方都合法**，错在**几何顶点格式与材质需求对不上**
//   （网格做错了 / 顶点格式选错了），数据层拿不到 `geometry_vertex_format`，所以判在
//   ECS 渲染层（`RenderPrimitiveCollectSystem::ApplyGeometryErrorFallback`），判据本身是
//   `MaterialRecipe.h::ClassifyMaterialGeometryError`（纯函数，可被契约测试覆盖）。
//   ShaderGen 的面法线降级（`MaterialProgramAssembler::PrepareResolvedDefinition`）因此只剩
//   两种合法用途：① `normal_mode = OptionalFaceFallback` 的棋盘格自身；
//   ② 调用方没提供几何顶点格式的路径（离线/测试）兜底。
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
//   [MaterialFallback] 材质 "UnlitTexture" 声明为 required 的纹理缺少实际资源 => ...
//                      （棋盘格；错误种类 missing_texture_resource，颜色=(0.00,1.00,1.00,1.00)）
//   [MaterialFallback] 材质 "Lit" 与几何顶点格式不匹配（错误种类 missing_vertex_normal）
//                      => 回退错误材质 ... （棋盘格，颜色=(1.00,1.00,0.00,1.00)）
//   [MaterialFallback] 材质 "Lit" 与几何顶点格式不匹配（错误种类 missing_texture_uv）
//                      => 回退错误材质 ... （棋盘格，颜色=(0.20,0.20,1.00,1.00)）
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
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/ssbo/LitMaterialData.h>
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
    constexpr int   kCaseCount    = 9;
    constexpr float kSphereRadius = 1.0f;
    constexpr float kSpacing      = 3.2f;
    constexpr int   kCaseColumns  = 3;
    constexpr int   kCaseRows     = 3;
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
        bool                    omit_vertex_normal; ///< true = 几何顶点格式**不含** Normal
        bool                    omit_vertex_texcoord;///< true = 几何顶点格式**不含** TexCoord
        bool                    use_pbr_surface_row;///< true = 配方要挂 PBRSurface 数据行
    };

    constexpr ErrorCaseDesc kErrorCases[kCaseCount] =
    {
        { "00_Control",     "EF.Control",  "builtin/checkerboard_3d",
          "None",                     "灰度(0.00/0.35)",
          "对照组：正常引用零依赖棋盘格（不读标记色）",
          false, false, false },

        { "01_EmptyID",     "EF.EmptyID",  "",
          "MissingMaterialSource",    "洋红(1.00,0.00,1.00)",
          "配方里 mtl_def_id 留空 ⇒ 无材质来源",
          false, false, false },

        { "02_NoSource",    "EF.NoSource", nullptr,
          "MissingMaterialSource",    "洋红(1.00,0.00,1.00)",
          "asset 的 PrimitiveVariant 为空 ⇒ 数据层与 asset 都没有配方",
          false, false, false },

        { "03_UnknownID",   "EF.UnknownID", "NotExist/Material",
          "UnknownMaterialDefinition", "橙(1.00,0.50,0.00)",
          "材质定义 ID 在材质库里不存在",
          false, false, false },

        { "04_TypoID",      "EF.TypoID",   "builtin/checkerbord_3d",
          "UnknownMaterialDefinition", "橙(1.00,0.50,0.00)",
          "builtin ID 拼写错（少一个 'a'）—— 最容易犯的错",
          false, false, false },

        { "05_CaseMismatch", "EF.CaseMismatch", "lit",
          "UnknownMaterialDefinition", "橙(1.00,0.50,0.00)",
          "大小写不符：真实定义是 \"Lit\"（ID 区分大小写）",
          false, false, false },

        { "06_MissingTexture", "EF.MissingTexture", "UnlitTexture",
          "MissingTextureResource",   "青(0.00,1.00,1.00)",
          "材质定义声明 base_color required=true，但配方一个纹理资源都没给",
          false, false, false },

        { "07_NoVertexNormal", "EF.NoVertexNormal", "Lit",
          "MissingVertexNormal",      "黄(1.00,1.00,0.00)",
          "几何只有 Position+TexCoord（无 Normal）⇒ 法线数据坏了，不能伪装成平面着色",
          true,  false, true },

        { "08_NoTexCoord", "EF.NoTexCoord", "Lit",
          "MissingTextureUV",         "蓝(0.20,0.20,1.00)",
          "几何只有 Position+Normal（无 TexCoord）⇒ 贴图会按垃圾 UV 采样",
          false, true,  true },
    };

    /// 常规球体：Position + TexCoord + Normal（`UnlitTexture` 要 UV0，`Lit` 要 Normal）。
    GeometryVertexFormat CreatePositionTexCoordNormalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            { VertexSemantic::Position, VF_V3F },
            { VertexSemantic::TexCoord, VF_V2F },
            { VertexSemantic::Normal,   VF_V3F },
        };
        return gvf;
    }

    /// 「顶点没有法线数据」演示专用：只有 Position + TexCoord（Lit 还要 UV0）。
    GeometryVertexFormat CreatePositionTexCoordGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            { VertexSemantic::Position, VF_V3F },
            { VertexSemantic::TexCoord, VF_V2F },
        };
        return gvf;
    }

    /// 「顶点没有 UV」演示专用：只有 Position + Normal（Lit 要 Normal，缺 TexCoord）。
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

    using MaterialDataAccessor = graph::GlobalSSBODataAccessor;

    Geometry *     geoms[kCaseCount]   = {};
    graph::mtl::MaterialRecipe recipes[kCaseCount] = {};
    PrimitiveAsset assets[kCaseCount]  = {};

    MaterialDataAccessor material_data_ssbo_accessor{};

    uint32_t exit_frames = 0;
    uint32_t frame_seen  = 0;

private:

    /// 「材质要法线光照但顶点没有 NTB」演示所需的 PBR 数据行（Lit 的私有数据资源）。
    bool InitMaterialDataSSBO()
    {
        auto * domain_manager = GetManager<GlobalSSBOBufferRegistry>();

        if (!domain_manager)
        {
            GLogError(u8"[MaterialErrorFallback] 没有 SSBO 注册表");
            return false;
        }

        material_data_ssbo_accessor =
            domain_manager->GetAccessor<graph::ssbo::PBRSurfaceRow>();

        if (!material_data_ssbo_accessor)
        {
            GLogError(u8"[MaterialErrorFallback] 取不到 PBRSurfaceRow 访问器");
            return false;
        }

        graph::ssbo::PBRSurfaceRow material_data{};
        material_data.base_color = Color4f(0.85f, 0.72f, 0.45f, 1.0f);
        material_data.metallic   = 0.05f;
        material_data.roughness  = 0.85f;

        return material_data_ssbo_accessor.Write(material_data);
    }

    bool CreateGeometries()
    {
        auto * device           = GetDevice();
        auto * geometry_manager = GetManager<GeometryManager>();

        if (!device || !geometry_manager)
        {
            GLogError(u8"[MaterialErrorFallback] 没有 Vulkan 设备或几何管理器");
            return false;
        }

        GeometryCreater standard_creater(device,
                                        CreatePositionTexCoordNormalGeometryVertexFormat());
        GeometryCreater no_normal_creater(device,
                                         CreatePositionTexCoordGeometryVertexFormat());
        GeometryCreater no_texcoord_creater(device,
                                           CreatePositionNormalGeometryVertexFormat());

        for (int i = 0; i < kCaseCount; ++i)
        {
            const bool no_normal   = kErrorCases[i].omit_vertex_normal;
            const bool no_texcoord = kErrorCases[i].omit_vertex_texcoord;

            GeometryCreater * creater = &standard_creater;

            if (no_normal)
                creater = &no_normal_creater;
            else if (no_texcoord)
                creater = &no_texcoord_creater;

            // 缺法线的演示用低面数球体：只是为了能看清 cross(dFdx,dFdy) 真的按三角形
            // 出法线（错误棋盘格的三平面投影不受面数影响）。
            geoms[i] = inline_geometry::CreateSphere(creater, no_normal ? 12 : 24);

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

        // Lit 的材质私有数据（PBRSurface）走材质数据行 —— 与其它 Lit 示例同一条路径。
        // 注意：这里**故意不设任何纹理**（Lit 的 6 张纹理全是 required=false）⇒
        // 不触发 MissingTextureResource 判定；#07/#08 要验证的是**几何侧**错配，
        // 所以配方本身必须完全合法。
        if (desc.use_pbr_surface_row)
            recipe.material_ssbo_binding =
                material_data_ssbo_accessor.GetGlobalSSBOBinding();

        // #06：故意**不**给 base_color 纹理资源 —— UnlitTexture 声明了
        // `required = true`，缺资源 ⇒ 数据层分类为 MissingTextureResource ⇒ 青棋盘格。
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

            // Z-up：网格铺开（列沿 +Y、行沿 +Z），保证一屏内全部可见。
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
        camera->distance       = 15.0f;
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
        GLogInfo(u8"[MaterialErrorFallback] 共 %d 个球体，%d 行 %d 列（Z-up 世界，列沿 +Y、行沿 +Z）",
                 kCaseCount, kCaseRows, kCaseColumns);

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
        if (!InitMaterialDataSSBO()) return false;
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
