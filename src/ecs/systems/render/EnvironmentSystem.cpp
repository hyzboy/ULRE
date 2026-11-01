#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/asset/GeometryAsset.h>
#include<hgl/graph/asset/GeometryAssetRegistry.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialTextureProjection.h>
#include<hgl/mtl/ScenePassProfileResolver.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/utf.h>
#include<hgl/log/Log.h>

namespace hgl::ecs
{
    EnvironmentSystem::EnvironmentSystem(const std::string &name)
        : System(name)
    {
        SetExecutionPhase(ExecutionPhase::RenderPreBeginFrame);
    }

    EnvironmentSystem::~EnvironmentSystem() = default;

    graph::EnvProfileID EnvironmentSystem::ResolveProfileID() const
    {
        // Env 随世界（C2）：profile 由**世界**持有（世界初始化时取本世界 RT 的 env_profile，
        // 也可由世界显式 SetEnvProfileID / CreateEnvProfile）——不再每次去问 RT。
        if (context)
            return context->GetEnvProfileID();

        return graph::kEnvProfileDefault;
    }

    graph::EnvironmentManager *EnvironmentSystem::ResolveManager()
    {
        if (render_context)
        {
            if (auto *gc = render_context->GetGraphicsContext())
                return gc->GetEnvironmentManager();
        }

        if (context)
        {
            if (auto *rc = context->GetRenderContext())
            {
                if (auto *gc = rc->GetGraphicsContext())
                    return gc->GetEnvironmentManager();
            }

            if (auto *gc = context->GetGraphicsContext())
                return gc->GetEnvironmentManager();
        }

        return nullptr;
    }

    // ── 环境光 provider 槽（`ambient_light_provider`）──
    //  ⚠ 原 `sky` 三件套（EditSkyInfo/GetSkyInfo/SetSkyInfo/MarkSkyDirty）已于 2026-10-07
    //  随 sky 段整删（两个字段全仓无写者 ⇒ 取值搬成 shader 常量；`addr_sky` 退出世界表）。

    graph::AmbientEnvInfo *EnvironmentSystem::EditAmbientEnvInfo()
    {
        auto *manager = ResolveManager();
        if (!manager)
            return nullptr;

        auto *info = manager->Edit(ResolveProfileID());
        if (!info)
            return nullptr;

        return &info->ambient_env;
    }

    void EnvironmentSystem::MarkAmbientEnvDirty()
    {
        if (auto *manager = ResolveManager())
            manager->MarkDirty(ResolveProfileID());
    }

    void EnvironmentSystem::SetLDRExposure(bool enable, float base_exposure, float sun_scale, float min_ambient)
    {
        if (auto *info = EditAmbientEnvInfo())
        {
            info->ldr_param.x = enable ? 1.0f : 0.0f;
            info->ldr_param.y = base_exposure;
            info->ldr_param.z = sun_scale;
            info->ldr_param.w = min_ambient;
            MarkAmbientEnvDirty();
        }
    }

    // ── ambient provider 模块选择（算法轴）──
    //  与 `EditAmbientEnvInfo()`（数据轴）配对：环境决定"用哪个算法 + 喂什么数据"。
    bool EnvironmentSystem::SetAmbientProvider(const AnsiString &module_name)
    {
        auto *manager = ResolveManager();
        if (!manager)
            return false;

        return manager->SetAmbientProvider(ResolveProfileID(), module_name);
    }

    const AnsiString &EnvironmentSystem::GetAmbientProvider() const
    {
        static const AnsiString empty;

        auto *manager = const_cast<EnvironmentSystem *>(this)->ResolveManager();
        if (!manager)
            return empty;

        return manager->GetAmbientProvider(ResolveProfileID());
    }

    // ═══════════════════════════════════════════════════════════════════════
    // 天空 + 天光统一托管
    // ═══════════════════════════════════════════════════════════════════════

    graph::GraphicsContext *EnvironmentSystem::ResolveGraphicsContext()
    {
        if (render_context)
            if (auto *gc = render_context->GetGraphicsContext())
                return gc;

        if (context)
        {
            if (auto *rc = context->GetRenderContext())
                if (auto *gc = rc->GetGraphicsContext())
                    return gc;

            return context->GetGraphicsContext();
        }

        return nullptr;
    }

    namespace
    {
        /**
         * 贴图**动态范围**判定（决定 LDR 曝光调制是否自动生效）。
         *
         * 引擎里没有现成的 `IsHDRFormat`，只能按 `VkFormat` 显式列表判定：浮点 / 共享
         * 指数 / BC6H 算 HDR，其余 unorm·snorm 算 LDR。判据取**装载后的实际格式**而不是
         * 文件名 —— 同一批烘焙数据的 `.TexCube` 与 `.TexOct` 既可以存浮点也可以存量化版，
         * 只有格式能回答"高光动态范围丢了没有"。
         */
        bool IsHDRTextureFormat(const VkFormat fmt) noexcept
        {
            switch (fmt)
            {
                case VK_FORMAT_R32G32B32A32_SFLOAT:
                case VK_FORMAT_R32G32B32_SFLOAT:
                case VK_FORMAT_R16G16B16A16_SFLOAT:
                case VK_FORMAT_R16G16B16_SFLOAT:
                case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
                case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
                case VK_FORMAT_BC6H_UFLOAT_BLOCK:
                case VK_FORMAT_BC6H_SFLOAT_BLOCK:
                    return true;
                default:
                    return false;
            }
        }

        /// 半球穹顶要放大到能包住相机与场景（与 `SkyLambertDome` 示例一致）。
        constexpr float kDomeSkyScale   = 4.0f;
        constexpr float kSphereSkyScale = 1.0f;

        /// `ResolveMaterialTextureProjection` 只吃 `const char*`，而资源 id 在引擎里是
        /// UTF-8（`u8char`）。后缀判定是纯 ASCII ⇒ 直接重解释即可，不做字符集转换。
        inline graph::mtl::MaterialTextureProjection ResolveProjectionOf(const U8String &uri)
        {
            return graph::mtl::ResolveMaterialTextureProjection(
                reinterpret_cast<const char *>(uri.c_str()));
        }
    }

    uint32 EnvironmentSystem::LoadSkyTexture(const OSString &uri, uint32 *mip_out, const graph::Texture **tex_out)
    {
        if (mip_out)
            *mip_out = 0;

        if (tex_out)
            *tex_out = nullptr;

        if (uri.IsEmpty())
            return 0;

        auto *gc = ResolveGraphicsContext();
        if (!gc)
        {
            GLogError(u8"[EnvironmentSystem] 没有图形上下文，无法装载天空贴图");
            return 0;
        }

        auto *texture_manager = gc->GetTextureManager();
        auto *bindless        = gc->GetBindlessTextureManager();
        if (!texture_manager || !bindless)
        {
            GLogError(u8"[EnvironmentSystem] TextureManager / BindlessTextureManager 不可用");
            return 0;
        }

        const U8String u8_uri = ToU8String(uri);

        // 装载形态由**后缀**决定，与渲染侧判定投影的规则同源（`ResolveMaterialTextureProjection`）：
        //   `.TexCube` → 立方体容器 → LoadTextureCube
        //   `.TexOct`  → 八面体展开图，仍是 **2D 容器**（单层 + 完整 mip 链）→ LoadTexture2D
        //   其余        → 普通 2D → LoadTexture2D
        // 八面体绝不能走 LoadTextureCube：那会把每层当 cube face，装出来是垃圾。
        graph::Texture *texture = (ResolveProjectionOf(u8_uri)
                                        == graph::mtl::MaterialTextureProjection::CubeMap)
            ? static_cast<graph::Texture *>(texture_manager->LoadTextureCube(uri, false))
            : static_cast<graph::Texture *>(texture_manager->LoadTexture2D(uri, false));

        if (!texture)
        {
            GLogError(u8"[EnvironmentSystem] 天空贴图装载失败：%s", u8_uri.c_str());
            return 0;
        }

        const uint32 handle = bindless->RegisterTexture(texture);
        if (handle == 0)
        {
            GLogError(u8"[EnvironmentSystem] 天空贴图 bindless 注册失败：%s", u8_uri.c_str());
            return 0;
        }

        if (mip_out)
            *mip_out = texture->GetMipLevel();

        if (tex_out)
            *tex_out = texture;

        return handle;
    }

    void EnvironmentSystem::SetDefaultSkyGradientSH()
    {
        // 没有显式烘焙数据时的兜底：无论配置里写的是什么，都按默认解析天空重算一次。
        sky_config.sh_auto = true;
        WriteSkySHFromMainLight();
    }

    bool EnvironmentSystem::UpdateSkySHFromSun()
    {
        if (!sky_config.sh_auto || !sky_config.sh_track_sun)
            return false;

        return WriteSkySHFromMainLight();
    }

    bool EnvironmentSystem::WriteSkySHFromMainLight()
    {
        auto *info = EditAmbientEnvInfo();
        if (!info)
        {
            GLogWarning(u8"[EnvironmentSystem] 没有 ambient_env 参数块，SH 系数未写入");
            return false;
        }

        // 主方向光 = 光源行 0（`kMainLightRowIndex`）。光源表可能还没初始化
        //（`Ensure()` 尚未被任何光源调用触发），此时按"正顶太阳、零强度"合成：
        // 天光只剩天空渐变，不会因为"没有光源系统"而黑屏。
        math::Vector4f sun_dir(0.0f, 0.0f, 1.0f, 0.0f);
        math::Vector4f sun_color(1.0f, 0.98f, 0.95f, 1.0f);
        float sun_intensity = 0.0f;

        if (context)
        {
            const graph::LightRow *row =
                context->GetLightRowTable().Row(graph::kMainLightRowIndex);

            if (row)
            {
                sun_dir       = row->direction_intensity;
                sun_color     = row->color_angle;
                sun_intensity = row->direction_intensity.w;
            }
        }

        // 光"没变"就跳过：静态光场景不会有任何重算与标脏。
        // 方向按分量比较（方向光的 `.xyz` 本身就是单位向量，不必归一化），
        // 同时比颜色与强度 —— 白天到夜里强度会从 1 掉到 0，只盯方向的话
        // 「太阳没动、只是被调暗」这种场景天光会一直停在亮的那一版。
        const float dx = sun_dir.x - sky_last_sun_dir.x;
        const float dy = sun_dir.y - sky_last_sun_dir.y;
        const float dz = sun_dir.z - sky_last_sun_dir.z;

        const float dr = sun_color.x - sky_last_sun_color.x;
        const float dg = sun_color.y - sky_last_sun_color.y;
        const float db = sun_color.z - sky_last_sun_color.z;

        const float di = sun_intensity - sky_last_sun_intensity;

        const float delta = dx * dx + dy * dy + dz * dz
                          + dr * dr + dg * dg + db * db
                          + di * di;

        const bool sun_changed = delta > (1.0f / 4096.0f);

        if (sky_sh_written && !sun_changed)
            return false;

        // 仰角正弦：Z-up 世界里就是单位太阳方向的 z 分量。
        const float len = sqrtf(sun_dir.x * sun_dir.x + sun_dir.y * sun_dir.y + sun_dir.z * sun_dir.z);
        const float elevation_sin = (len > 1e-6f) ? (sun_dir.z / len) : 1.0f;

        graph::SkySH9Params params = sky_config.sh_params;

        // 三段色随仰角走：日落变暖、夜里变暗 —— 这是"天光自动跟着太阳调"的核心。
        graph::EvaluateSkyGradientForSun(elevation_sin,
                                         params.zenith_color,
                                         params.horizon_color,
                                         params.ground_color);

        params.sun_direction  = sun_dir;
        params.sun_color      = sun_color;
        params.sun_irradiance = sun_intensity * sky_config.sh_sun_irradiance_scale;

        graph::ComputeSkySH9(params, info->sh);

        sky_last_sun_dir       = sun_dir;
        sky_last_sun_color     = sun_color;
        sky_last_sun_intensity = sun_intensity;
        sky_sh_written         = true;

        MarkAmbientEnvDirty();
        return true;
    }

    bool EnvironmentSystem::SetupSkyEnvironment(const SkyEnvironmentConfig &config)
    {
        if (!context)
        {
            GLogError(u8"[EnvironmentSystem] SetupSkyEnvironment 无 ECS 上下文");
            return false;
        }

        // 重复调用 = 先拆再建：环境热切不该留下上一套的天空实体或过期句柄。
        DestroySkyEnvironment();

        auto *gc = ResolveGraphicsContext();
        if (!gc)
        {
            GLogError(u8"[EnvironmentSystem] SetupSkyEnvironment 无图形上下文");
            return false;
        }

        sky_config = config;

        // ── 0. `None`：显式声明"没有天空、也没有天光" ──
        //
        // 走一条早退路径而不是让下面的步骤各自判一次：`DestroySkyEnvironment()`
        // 已经把数据轴（`env_tex` / `ldr_param` / `sh`）清零，这里只需要把算法轴
        // 钉到空对象 provider。前向材质仍然编译出 `ambient_light_provider` 槽，
        // `EvalIndirectLighting()` 恒返回 0 —— "没有天光"是一条数据，不是一条分支。
        if (sky_config.mode == SkyMode::None)
        {
            if (sky_config.HasSkyVisual() || sky_config.HasIBLData())
                GLogWarning(u8"[EnvironmentSystem] mode=None 忽略 sky_recipe/IBL 贴图：该模式只声明无天光");

            const AnsiString provider(graph::mtl::kAmbientLightProviderNone);

            if (!SetAmbientProvider(provider))
            {
                GLogError(u8"[EnvironmentSystem] ambient provider 切换失败：%s", provider.c_str());
                DestroySkyEnvironment();
                return false;
            }

            MarkAmbientEnvDirty();

            sky_managed = true;

            GLogInfo(u8"[EnvironmentSystem] 天空就绪：mode=%s provider=%s（无天光）",
                     GetSkyModeName(sky_config.mode), provider.c_str());

            return true;
        }

        // ── 1. 视觉天空实体（可缺省：只想要天光时配方留空）──
        if (sky_config.HasSkyVisual())
        {
            const char *geometry_uri = sky_config.sky_geometry_uri.IsEmpty()
                ? GetSkyModeGeometryURI(sky_config.mode)
                : sky_config.sky_geometry_uri.c_str();

            if (!geometry_uri)
            {
                GLogError(u8"[EnvironmentSystem] 天空模式 %s 没有默认几何体，且未指定 sky_geometry_uri",
                          GetSkyModeName(sky_config.mode));
                DestroySkyEnvironment();
                return false;
            }

            // 顶点流按模式声明：半球穹顶的顶点自带 UV（UV 就是方位投影结果），
            // 全天球只有位置（方向由片元侧从位置反推）。多声明/少声明属性都会让
            // 顶点输入布局与几何体不一致。
            graph::GeometryVertexFormat sky_format;
            sky_format.Add(graph::VertexSemantic::Position, VF_V3F);

            if (SkyModeGeometryHasUV(sky_config.mode))
                sky_format.Add(graph::VertexSemantic::TexCoord, VF_V2F);

            auto *registry = gc->GetGeometryAssetRegistry();
            if (!registry)
            {
                GLogError(u8"[EnvironmentSystem] 没有几何资产注册器");
                DestroySkyEnvironment();
                return false;
            }

            // 走**资产句柄**而不是裸 `Geometry*`：`PrimitiveAsset` 有了句柄就能在
            // 几何尚未就绪时被渲染门禁挡下，而不是拿半个几何去画。
            const graph::GeometryAsset *geometry_asset =
                registry->GetOrLoadURI(ToOSString(geometry_uri), &sky_format);

            if (!geometry_asset || !geometry_asset->GetGeometry())
            {
                GLogError(u8"[EnvironmentSystem] 天空几何体装载失败：%s", geometry_uri);
                DestroySkyEnvironment();
                return false;
            }

            const graph::mtl::MaterialRecipe *recipe =
                gc->FindOrLoadMaterialRecipe(sky_config.sky_recipe.c_str());

            if (!recipe)
            {
                GLogError(u8"[EnvironmentSystem] 天空材质配方装载失败：%s",
                          sky_config.sky_recipe.c_str());
                DestroySkyEnvironment();
                return false;
            }

            Entity *sky_entity = context->CreateEntity<Entity>("SkyEnvironment");
            if (!sky_entity)
            {
                DestroySkyEnvironment();
                return false;
            }

            sky_entity_id = sky_entity->GetEntityID();

            sky_variant = graph::PrimitiveVariant{};
            sky_variant.material_recipe = recipe;
            sky_variant.purpose         = graph::PrimitiveVariantPurpose::Surface;

            // `GeometryData` 只存 `PrimitiveAsset*`，所以 `sky_primitive` / `sky_variant`
            // 必须是本系统的成员（活到实体销毁），不能用栈上临时对象。
            sky_primitive = graph::PrimitiveAsset(geometry_asset->GetGeometry(),
                                                 geometry_asset,
                                                 &sky_variant,
                                                 1,
                                                 graph::PrimitiveType::Triangles);

            context->GetOrCreateGeometryData(sky_entity)->SetPrimitiveAsset(&sky_primitive);
            context->GetOrCreateMaterialData(sky_entity);

            const float scale = sky_config.sky_scale > 0.0f
                ? sky_config.sky_scale
                : (SkyModeGeometryHasUV(sky_config.mode) ? kDomeSkyScale : kSphereSkyScale);

            // Mobility 保持 Movable：`track_camera` 要逐帧改它的世界位置（跟随相机 ≠ 静态）。
            auto transform = context->GetTransform(
                context->CreateTransform(sky_entity_id, Mobility::Movable));

            transform.SetLocalPosition(glm::vec3(0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(scale));
        }

        // ── 2. 天光数据（IBL 三张图 / SH 系数，二者互斥）──
        auto *ambient_env = EditAmbientEnvInfo();
        if (!ambient_env)
        {
            GLogError(u8"[EnvironmentSystem] 没有 ambient_env 参数块（env profile 未就绪？）");
            DestroySkyEnvironment();
            return false;
        }

        uint32 irradiance_handle = 0;
        uint32 prefilter_handle  = 0;
        uint32 brdf_lut_handle   = 0;
        uint32 prefilter_mips    = 0;

        if (sky_config.HasIBLData())
        {
            irradiance_handle = LoadSkyTexture(sky_config.irradiance_texture, nullptr, &sky_irradiance_tex);
            prefilter_handle  = LoadSkyTexture(sky_config.prefilter_texture, &prefilter_mips, &sky_prefilter_tex);
            brdf_lut_handle   = LoadSkyTexture(
                sky_config.brdf_lut_texture.IsEmpty()
                    ? OSString(OS_TEXT("res/image/brdflut.Tex2D"))
                    : sky_config.brdf_lut_texture,
                nullptr,
                nullptr);

            if (!irradiance_handle || !prefilter_handle || !brdf_lut_handle)
            {
                GLogError(u8"[EnvironmentSystem] IBL 天光贴图未全部就绪");
                DestroySkyEnvironment();
                return false;
            }
        }

        // 句柄槽位 x=辐照度 / y=预滤波 / z=BRDF LUT / w=预滤波 mip 级数。
        // w 是 shader 把 roughness 换成 LOD 的唯一依据：lod = roughness × (w-1)，与
        // TexConv 的「第 i 级 = i/(级数-1)」互逆 ⇒ 级数取自贴图自身头部，重烘不用改这里。
        ambient_env->env_tex = math::Vector4u(irradiance_handle,
                                              prefilter_handle,
                                              brdf_lut_handle,
                                              prefilter_mips);

        // ── 3. LDR / HDR：只有 "丢了高光动态范围" 的数据才提亮 ──
        bool ldr_enable = (sky_config.ldr == SkyAmbientLDR::On);

        if (sky_config.ldr == SkyAmbientLDR::Auto)
        {
            // 判据落在**装载出来的格式**上：浮点烘焙图是物理正确的亮度，提亮反而错；
            // unorm 量化图少了高光，不提亮就会明显偏黑。没有 IBL 数据（纯 SH）不提亮。
            ldr_enable = false;

            if (sky_config.HasIBLData())
            {
                const VkFormat fmt = sky_irradiance_tex
                    ? sky_irradiance_tex->GetFormat()
                    : VK_FORMAT_UNDEFINED;

                ldr_enable = !IsHDRTextureFormat(fmt);
            }
        }

        SetLDRExposure(ldr_enable,
                       sky_config.ldr_exposure_base,
                       sky_config.ldr_sun_scale,
                       sky_config.ldr_min_ambient);

        // ── 4. SH9 天光数据（与 IBL 句柄**并行**而非互斥）──
        //
        // `ambient_light_sh` 只认 `sh[9]`，所以无论最终选哪个 provider，都把它写好：
        // 调用方在 IBL / SH 之间热切时不需要重新 Setup，也不会切到一组陈旧（甚至全 0）系数。
        if (sky_config.sh_auto)
        {
            // 解析合成：三段色 × 太阳方向 ⇒ 闭式 SH 投影（太阳从行 0 现取）。
            WriteSkySHFromMainLight();
        }
        else if (!sky_config.IsSHZero())
        {
            // 重新取一次指针：上面的 `SetLDRExposure()` 也走过 `EditAmbientEnvInfo()`。
            if (auto *sh_info = EditAmbientEnvInfo())
            {
                for (int i = 0; i < 9; ++i)
                    sh_info->sh[i] = sky_config.sh[i];
            }

            // 显式系数也参与"太阳动了要不要重算"的记账，避免之后的 Update 把它覆盖掉。
            sky_sh_written = true;
        }
        else if (!irradiance_handle && !prefilter_handle
                 && sky_config.mode != SkyMode::Atmosphere)
        {
            SetDefaultSkyGradientSH();      // 既无 IBL 也无显式系数 ⇒ 兜底成解析天空梯度，避免黑球
        }

        // ── 5. 天光算法（模块槽），与数据轴配对选择 ──
        AnsiString provider(sky_config.ambient_provider);

        if (provider.IsEmpty())
        {
            if (irradiance_handle && prefilter_handle)
            {
                // 立方体与八面体共用同一套句柄语义，只换算法模块名；材质与光照模型一行不改。
                const U8String u8_prefilter = ToU8String(sky_config.prefilter_texture);

                provider = (ResolveProjectionOf(u8_prefilter)
                                == graph::mtl::MaterialTextureProjection::Octahedral)
                    ? AnsiString("ambient_light_ibl_oct")
                    : AnsiString("ambient_light_ibl");
            }
            else if (sky_config.mode == SkyMode::Atmosphere)
            {
                // 解析大气天空的低画质间接光：以世界法线方向采样天空色，无需任何探针。
                provider = AnsiString("ambient_light_sky");
            }
            else
            {
                provider = AnsiString("ambient_light_sh");
            }
        }

        if (!SetAmbientProvider(provider))
        {
            GLogError(u8"[EnvironmentSystem] ambient provider 切换失败：%s", provider.c_str());
            DestroySkyEnvironment();
            return false;
        }

        MarkAmbientEnvDirty();

        sky_managed = true;

        GLogInfo(u8"[EnvironmentSystem] 天空就绪：mode=%s recipe=%s provider=%s ldr=%d mips=%u",
                 GetSkyModeName(sky_config.mode),
                 sky_config.sky_recipe.IsEmpty() ? "(none)" : sky_config.sky_recipe.c_str(),
                 provider.c_str(),
                 ldr_enable ? 1 : 0,
                 prefilter_mips);

        return true;
    }

    void EnvironmentSystem::DestroySkyEnvironment()
    {
        if (sky_entity_id != EntityID::Invalid())
        {
            if (context)
                context->DestroyEntity(sky_entity_id);

            sky_entity_id = EntityID::Invalid();
        }

        // 先把指向成员变体的句柄断开，再清变体本身。
        sky_primitive = graph::PrimitiveAsset{};
        sky_variant   = graph::PrimitiveVariant{};

        sky_irradiance_tex = nullptr;
        sky_prefilter_tex  = nullptr;

        sky_managed     = false;
        sky_sh_written  = false;
        sky_last_sun_dir       = math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
        sky_last_sun_color     = math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
        sky_last_sun_intensity = 0.0f;

        // 天光数据轴清零：句柄全 0 + SH 全 0 + 关闭 LDR 调制。
        if (auto *info = EditAmbientEnvInfo())
        {
            info->env_tex   = math::Vector4u(0);
            info->ldr_param = math::Vector4f(0.0f);

            for (int i = 0; i < 9; ++i)
                info->sh[i] = math::Vector4f(0.0f);

            MarkAmbientEnvDirty();
        }

        // 算法轴回到规范兜底（`ambient_light_none`），而不是清成空串：空串等于
        // "让模板自己猜"，前向材质的 program 身份也会跟着落到"不折入 provider"
        // 的那一档，与"显式无天光"分裂成两个 program。
        if (GetAmbientProvider() != AnsiString(graph::mtl::kAmbientLightProviderNone))
            SetAmbientProvider(AnsiString(graph::mtl::kAmbientLightProviderNone));

        sky_config = SkyEnvironmentConfig{};
    }

    void EnvironmentSystem::Update(float delta_time)
    {
        (void)delta_time;

        if (!sky_managed || !context)
            return;

        // ── 太阳耦合：天光随主光源重算 ──
        // 天空视觉（`sky_atmosphere.glsl`）与 LDR 曝光调制（`ambient_light_ibl*.glsl`）
        // 都在 GPU 侧直接读光源行 0，逐帧自动跟手；只有 SH9 系数是 CPU 侧合成，
        // 太阳移动时必须在这里重算，否则日落了天光还是白的。
        UpdateSkySHFromSun();

        if (!sky_config.track_camera || sky_entity_id == EntityID::Invalid())
            return;

        // 天空是无视差的"无限远"背景：把它的世界位置钉到主相机。
        // 用 `camera_world_pos`（绝对世界坐标）而不是 `pos`——Camera-Relative Rendering
        // 下 `pos` 是相对量，直接拿去当世界位置会让天空漂到别处。
        const graph::CameraInfo *camera = context->ResolveActiveCameraInfo();
        if (!camera)
            return;

        const math::Vector3f &pos = camera->camera_world_pos;

        auto transform = context->GetTransformByEntity(sky_entity_id);
        if (!transform.IsValid())
            return;

        transform.SetWorldPosition(glm::vec3(pos.x, pos.y, pos.z));
    }
}//namespace hgl::ecs
