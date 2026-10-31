// MaterialErrorFallback
//
// 演示「材质配置写错 ⇒ 材质系统回退到错误棋盘格」：**不同错误种类用不同标记色**，
// 屏幕上肉眼就能分辨是哪一类配置错了。
//
// 定义层（`.definition.toml`）已整体移除：配方（recipe）里直接声明 `domain`，
// 材质定义由「领域契约 + 配方」当场合成，所以本示例的错误来源一律是**领域**写错，
// 不再是「材质定义 ID 引用不到」。
//
// 场景（引擎世界是 Z-up；9 个球体按 3 行 3 列铺开，相机从 -X 侧看）：
//   #0 对照        domain = "Checker3D"              正常                    → 灰度棋盘格
//   #1 未声明领域   配方在，但 domain 不写（Custom）   MissingMaterialSource   → 洋红棋盘格
//   #2 无材质来源   不设 asset 配方                    MissingMaterialSource   → 洋红棋盘格
//   #3 未知领域名   domain = "NotExistDomain"         UnknownMaterialDomain   → 橙棋盘格
//   #4 拼错领域名   domain = "StandradOpaque"（少一个 'd'）
//                                                    UnknownMaterialDomain   → 橙棋盘格
//   #5 大小写不符   domain = "standardopaque"（真实是 "StandardOpaque"）
//                                                    UnknownMaterialDomain   → 橙棋盘格
//   #6 缺纹理       domain = "UnlitTexture"（该领域 base_color
//                   binding_required=true），但配方不给任何纹理资源
//                                                    MissingTextureResource   → 青棋盘格
//   #7 缺顶点法线   domain = "StandardOpaque"，几何顶点只有 Position+TexCoord（**没有 Normal**）
//                                                    MissingVertexNormal      → 黄棋盘格
//   #8 缺纹理UV     domain = "StandardOpaque"，几何顶点只有 Position+Normal（**没有 TexCoord**）
//                                                    MissingTextureUV         → 蓝棋盘格
//
// #7/#8 的语义（用户 2026-10-24 两次拍板，后者覆盖前者）：
//   初版口径是「材质要法线光照、顶点没 NTB ⇒ 三角形平面渲染，这是引擎支持的**正常光照
//   模式**，所以只是降级、不算错误」。被推翻 —— 面法线平面着色看起来「像正常的低模」，
//   法线数据坏了这件事就被完全掩掉了；缺 UV 则按垃圾 UV 采样，表现为随机花纹，比没贴图
//   更难查。**两类都必须肉眼一眼可见** ⇒ 一律回退错误棋盘格，只用颜色区分种类。
//
//   这两类与 #1~#6 的区别：**领域契约与配方都合法**，错在**几何顶点格式与材质需求对不上**
//   （网格做错了 / 顶点格式选错了），数据层拿不到 `geometry_vertex_format`，所以判在
//   ECS 渲染层（`RenderPrimitiveCollectSystem::ApplyGeometryErrorFallback`），判据本身是
//   `MaterialRecipe.h::ClassifyMaterialGeometryError`（纯函数，可被契约测试覆盖）。
//   ShaderGen 的面法线降级（`MaterialProgramAssembler::PrepareResolvedDefinition`）因此只剩
//   两种合法用途：① `normal_mode = OptionalFaceFallback` 的棋盘格自身；
//   ② 调用方没提供几何顶点格式的路径（离线/测试）兜底。
//
// 回退机制与本表的定义中枢见 `inc/hgl/mtl/MaterialRecipe.h`：
//   · `kFallbackMaterialRules`      —— 错误种类 → 保底**领域** + 标记色 + 稳定名
//   · `kMarkerlessFallbackMaterialDomain` —— 标记色行拿不到时的零依赖降级领域
// 运行期把标记色挂到配方上的唯一入口是
// `RenderPrimitiveCollectSystem::AttachFallbackMarkerColor`（走与保底纯色配方
// 完全相同的 `EmissiveSurface.color` → `recipe.material_ssbo_binding` 路径）。
//
// 期望日志（每个错误实体一条）：
//   [MaterialFallback] 未声明材质领域（domain 缺失）: "EF.UnknownDomain" => 回退错误材质
//                      ErrorChecker3D（领域；错误种类 missing_material_source，
//                      颜色=(1.00,0.00,1.00,1.00)）
//   [MaterialFallback] 材质领域无契约（domain 非法）: "EF.UnknownDomain" => 回退错误材质
//                      ErrorChecker3D（领域；错误种类 unknown_material_domain，
//                      颜色=(1.00,0.50,0.00,1.00)）
//   [MaterialFallback] 材质 "UnlitTexture" 声明为 required 的纹理缺少实际资源 => ...
//                      （棋盘格；错误种类 missing_texture_resource，颜色=(0.00,1.00,1.00,1.00)）
//   [MaterialFallback] 材质 "StandardOpaque" 与几何顶点格式不匹配（错误种类 missing_vertex_normal）
//                      => 回退错误材质 ... （棋盘格，颜色=(1.00,1.00,0.00,1.00)）
//   [MaterialFallback] 材质 "StandardOpaque" 与几何顶点格式不匹配（错误种类 missing_texture_uv）
//                      => 回退错误材质 ... （棋盘格，颜色=(0.20,0.20,1.00,1.00)）
//
// 环境变量：
//   EF_FRAMES=N   跑 N 帧后退出（CI / 抓日志用）
//
// 注意：必须从仓库根目录启动（否则 ShaderLibrary 定位失败，材质库为空 ⇒ 所有材质
// 都落到 UnknownMaterialDomain，连对照实体也会变橙）。

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDomain.h>
#include<hgl/mtl/PipelineConfig.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
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
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/material_error_fallback.material_data.toml";
    constexpr uint32 MATERIAL_DATA_ROW = 0;
}

namespace
{
    constexpr int   kCaseCount    = 9;
    constexpr float kSphereRadius = 1.0f;
    constexpr float kSpacing      = 3.2f;
    constexpr int   kCaseColumns  = 3;
    constexpr int   kCaseRows     = 3;
    static_assert(kCaseColumns > 0 && kCaseRows > 0, "错误材质示例必须是网格布局");
    static_assert(kCaseColumns * kCaseRows == kCaseCount, "列 × 行必须等于示例数量");

    /// 领域名解析失败时写入的越界枚举值：引擎按下标取契约必定失败 ⇒ UnknownMaterialDomain。
    constexpr graph::mtl::MaterialDomain kUnresolvableMaterialDomain =
        static_cast<graph::mtl::MaterialDomain>(255);

    /// 每个演示例子的说明（只用于日志对照表）。
    struct ErrorCaseDesc
    {
        const char             *entity_name;
        const char             *recipe_name;
        /// 领域名三态：
        ///   nullptr      —— 不设 asset 配方（无材质来源）；
        ///   ""           —— 配方在，但**不声明领域**（Custom ⇒ MissingMaterialSource）；
        ///   其它字符串    —— 按名字解析；解析失败 ⇒ 非法领域值（UnknownMaterialDomain）。
        const char             *domain_name;
        const char             *expected_kind;
        const char             *expected_color;
        const char             *why;
        bool                    omit_vertex_normal; ///< true = 几何顶点格式**不含** Normal
        bool                    omit_vertex_texcoord;///< true = 几何顶点格式**不含** TexCoord
        bool                    use_pbr_surface_row;///< true = 配方要挂 PBRSurface 数据行
    };

    constexpr ErrorCaseDesc kErrorCases[kCaseCount] =
    {
        { "00_Control",     "EF.Control",  "Checker3D",
          "None",                     "灰度(0.00/0.35)",
          "对照组：正常声明零依赖棋盘格领域（不读标记色）",
          false, false, false },

        { "01_EmptyDomain", "EF.EmptyDomain",  "",
          "MissingMaterialSource",    "洋红(1.00,0.00,1.00)",
          "配方里不声明领域（domain 留空）⇒ 无材质来源",
          false, false, false },

        { "02_NoSource",    "EF.NoSource", nullptr,
          "MissingMaterialSource",    "洋红(1.00,0.00,1.00)",
          "asset 的 PrimitiveVariant 为空 ⇒ 数据层与 asset 都没有配方",
          false, false, false },

        { "03_UnknownDomain", "EF.UnknownDomain", "NotExistDomain",
          "UnknownMaterialDomain",    "橙(1.00,0.50,0.00)",
          "领域名在契约表里不存在",
          false, false, false },

        { "04_TypoDomain",  "EF.TypoDomain",   "StandradOpaque",
          "UnknownMaterialDomain",    "橙(1.00,0.50,0.00)",
          "领域名拼写错（少一个 'd'）—— 最容易犯的错",
          false, false, false },

        { "05_CaseMismatch", "EF.CaseMismatch", "standardopaque",
          "UnknownMaterialDomain",    "橙(1.00,0.50,0.00)",
          "大小写不符：真实领域名是 \"StandardOpaque\"（领域名区分大小写）",
          false, false, false },

        { "06_MissingTexture", "EF.MissingTexture", "UnlitTexture",
          "MissingTextureResource",   "青(0.00,1.00,1.00)",
          "领域契约声明 base_color binding_required=true，但配方一个纹理资源都没给",
          false, false, false },

        { "07_NoVertexNormal", "EF.NoVertexNormal", "StandardOpaque",
          "MissingVertexNormal",      "黄(1.00,1.00,0.00)",
          "几何只有 Position+TexCoord（无 Normal）⇒ 法线数据坏了，不能伪装成平面着色",
          true,  false, true },

        { "08_NoTexCoord", "EF.NoTexCoord", "StandardOpaque",
          "MissingTextureUV",         "蓝(0.20,0.20,1.00)",
          "几何只有 Position+Normal（无 TexCoord）⇒ 贴图会按垃圾 UV 采样",
          false, true,  true },
    };

    /// 常规球体：Position + TexCoord + Normal（`UnlitTexture` 要 UV0，`StandardOpaque` 要 Normal）。
    GeometryVertexFormat CreatePositionTexCoordNormalGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            { VertexSemantic::Position, VF_V3F },
            { VertexSemantic::TexCoord, VF_V2F },
            { VertexSemantic::Normal,   VF_V3F },
        };
        return gvf;
    }

    /// 「顶点没有法线数据」演示专用：只有 Position + TexCoord（StandardOpaque 还要 UV0）。
    GeometryVertexFormat CreatePositionTexCoordGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            { VertexSemantic::Position, VF_V3F },
            { VertexSemantic::TexCoord, VF_V2F },
        };
        return gvf;
    }

    /// 「顶点没有 UV」演示专用：只有 Position + Normal（StandardOpaque 要 Normal，缺 TexCoord）。
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

    graph::GlobalSSBOBinding material_data_ssbo_accessor{};

    uint32_t exit_frames = 0;
    uint32_t frame_seen  = 0;

private:

    /// 「材质要法线光照但顶点没有 NTB」演示所需的 PBR 数据行（StandardOpaque 的私有数据资源）。
    bool InitMaterialDataSSBO()
    {
        return GetMaterialDataBinding(MATERIAL_DATA_URI, MATERIAL_DATA_ROW, material_data_ssbo_accessor);
    }

    bool CreateGeometries()
    {
        if (!GetDevice())
        {
            GLogError(u8"[MaterialErrorFallback] 没有 Vulkan 设备");
            return false;
        }

        // 三种顶点布局对应三种"缺字段"场景；每几何一套私有缓冲 ⇒ 走 format_override
        static const GeometryVertexFormat standard_format  = CreatePositionTexCoordNormalGeometryVertexFormat();
        static const GeometryVertexFormat no_normal_format  = CreatePositionTexCoordGeometryVertexFormat();
        static const GeometryVertexFormat no_texcoord_format = CreatePositionNormalGeometryVertexFormat();

        for (int i = 0; i < kCaseCount; ++i)
        {
            const bool no_normal   = kErrorCases[i].omit_vertex_normal;
            const bool no_texcoord = kErrorCases[i].omit_vertex_texcoord;

            const GeometryVertexFormat * format = &standard_format;

            if (no_normal)
                format = &no_normal_format;
            else if (no_texcoord)
                format = &no_texcoord_format;

            // 缺法线的演示用低面数球体：只是为了能看清 cross(dFdx,dFdy) 真的按三角形
            // 出法线（错误棋盘格的三平面投影不受面数影响）。
            geoms[i] = GetGeometry(no_normal ? "geo:builtin:sphere_12" : "geo:builtin:sphere_24", format);

            if (!geoms[i])
            {
                GLogError(u8"[MaterialErrorFallback] 创建球体失败：index=%d", i);
                return false;
            }
        }

        return true;
    }

    /// 组装第 index 个例子的配方 + asset（`domain_name == nullptr` 时**不设**配方）。
    void BuildCaseAsset(const int index)
    {
        const ErrorCaseDesc & desc = kErrorCases[index];

        if (!desc.domain_name)
        {
            // 无材质来源：变体清单为空 ⇒ GeometryData 的 asset 默认配方为 nullptr，
            // 数据层也没有覆盖 ⇒ 分类为 MissingMaterialSource。
            assets[index] = PrimitiveAsset(geoms[index], nullptr, PrimitiveType::Triangles);
            return;
        }

        graph::mtl::MaterialRecipe & recipe = recipes[index];

        recipe.recipe_name = desc.recipe_name;
        recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();

        // #01：配方存在但不声明领域 ⇒ 无契约 ⇒ MissingMaterialSource。
        // #03~#05：领域名解析失败时故意写入**越界枚举值**，等价于「引用了不存在的领域」，
        //           引擎按下标取契约必定失败 ⇒ UnknownMaterialDomain。
        if (desc.domain_name && desc.domain_name[0])
        {
            graph::mtl::MaterialDomain domain = graph::mtl::MaterialDomain::Custom;

            if (!graph::mtl::ParseMaterialDomain(desc.domain_name, domain))
                domain = kUnresolvableMaterialDomain;

            recipe.domain = domain;
        }

        // StandardOpaque 的材质私有数据（PBRSurface）走材质数据行 —— 与其它 PBR 示例同一条路径。
        // 注意：这里**故意不设任何纹理**（该领域 6 张纹理全是 binding_required=false）⇒
        // 不触发 MissingTextureResource 判定；#07/#08 要验证的是**几何侧**错配，
        // 所以配方本身必须完全合法。
        if (desc.use_pbr_surface_row)
            recipe.material_ssbo_binding =
                material_data_ssbo_accessor;

        // #06：故意**不**给 base_color 纹理资源 —— UnlitTexture 契约里该槽声明为
        // `binding_required = true`，缺资源 ⇒ 数据层分类为 MissingTextureResource ⇒ 青棋盘格。
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

            GLogInfo(u8"[MaterialErrorFallback]   #%d %-16s domain=%-20s 期望种类=%-24s 期望颜色=%s  /* %s */",
                     i,
                     desc.entity_name,
                     desc.domain_name ? (desc.domain_name[0] ? desc.domain_name : "<未声明领域>")
                                      : "<无 asset 配方>",
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
        // geoms[] 由 GeometryAssetRegistry 持有，此处不再 delete，改为清空注册器。
        for (int i = 0; i < kCaseCount; ++i)
            geoms[i] = nullptr;

        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();
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
