#pragma once

#include<hgl/type/String.h>
#include<hgl/math/Vector.h>
#include<hgl/graph/sky/SkySH9.h>
#include<hgl/graph/asset/EnvironmentAssetSpec.h>

namespace hgl::ecs
{
    using namespace hgl::math;

    // 天空模式与 LDR 策略的**唯一定义**在环境资产规约里（`*.env.toml` 要按名写出它们），
    // 这里只做转出，避免同一个枚举出现两份定义、两份拼写表。
    using graph::SkyMode;
    using graph::SkyAmbientLDR;

    /**
     * 天空**技术档位**（画质选择的输入，不是资产内容）。
     *
     * 一个 `SkyEnvironmentConfig` 资产（schema 2 的 `*.env.toml`）里各技术段是**并列**
     * 存在的：高配用 `CubeMap`（HDR IBL）、低配用 `Octahedral` / `LambertDome`、
     * 超低配用 `SH9`。画质系统在外面**选一个档位**传进 `ApplyEnvironmentSpec()`，
     * 而不是往资产里配画质。
     *
     * `Default` = 不指定：按资产 `[ambient].mode` 的默认技术展开（等价于只装载
     * schema-1 遗留镜像，向后兼容）。
     */
    enum class SkyTechnique : uint8
    {
        CubeMap,        ///< `[cubemap]`：立方体贴图 IBL（`SkyMode::CubeMapIBL`）
        Octahedral,     ///< `[octahedral]`：八面体映射 IBL（`SkyMode::OctahedralIBL`）
        LambertDome,    ///< `[lambert_dome]`：半球穹顶 Lambert（`SkyMode::LambertDome`）
        Atmosphere,     ///< `[atmosphere]`：解析大气（`SkyMode::Atmosphere`）
        SH9,            ///< 顶层 `[sh9]`：纯球谐天光，无视觉天空（`SkyMode::SHLightOnly`）
        Default         ///< 用资产 `[ambient].mode` 指向的技术段
    };

    /// 技术名（诊断 / 日志用）。
    const char *GetSkyTechniqueName(SkyTechnique technique) noexcept;

    /**
     * 技术档位 → 视觉模式。`Default` 返回 `fallback`（一般是资产 `[ambient].mode`
     * 或调用方现配的 `cfg.mode`）。
     */
    SkyMode SkyTechniqueToSkyMode(SkyTechnique technique, SkyMode fallback) noexcept;

    // ═══════════════════════════════════════════════════════════════════════════
    // 统一天空 + 天光管理的**输入契约**
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 引擎里能画"天"的办法有好几种，它们各自拆成两件**正交**的事：
    //
    //   · 视觉天空（画什么几何体 + 用什么材质配方）—— `SkyMode` 只决定**默认几何体**；
    //   · 天光（`ambient_light_provider` 槽用哪个算法 + 喂什么数据）—— 由 IBL 贴图 /
    //     SH 系数 / 模式共同决定。
    //
    // 两者可以任意组合：例如"Octahedral 背景 + SH9 天光"，或"半球穹顶 Lambert 背景 +
    // 同一批烘好的 IBL 天光"。所以本结构不做成一张"模式 → 全部行为"的大表，而是
    // 分字段描述，`EnvironmentSystem::SetupSkyEnvironment()` 负责把它们落成实体与参数块。
    //
    // ⚠ 换**贴图投影形态**必须换**配方**（recipe 变体），不能在运行时改写配方里的
    //   `[[textures]] asset`：配方句柄与 `resource_id` 在装载时焊死，改写会让渲染侧每帧
    //   剔除（`DeferredResource` identity mismatch）。投影实现本身由 C++ 侧按资源后缀
    //   选择（`.TexCube` / `.TexOct` / 其余 2D），主着色器与表面模块没有任何分支。
    //
    // 枚举 `SkyMode` / `SkyAmbientLDR` 的定义已随环境资产规约迁到
    // `hgl/graph/asset/EnvironmentAssetSpec.h`，本头把它们转出到 `hgl::ecs`。

    /// 模式名（诊断 / 日志用）。
    const char *GetSkyModeName(SkyMode mode) noexcept;

    /// 该模式的默认几何体 URI；`SHLightOnly` 没有默认几何体 ⇒ 返回 nullptr。
    const char *GetSkyModeGeometryURI(SkyMode mode) noexcept;

    /// 该模式的天空几何体是否自带 UV（半球穹顶 = 是，全天球 = 否）。
    /// 顶点流按此选择：带 UV 的几何体声明 `Position|TexCoord`，否则只声明 `Position`。
    bool SkyModeGeometryHasUV(SkyMode mode) noexcept;

    /**
     * 天空 + 天光的完整配置。
     *
     * 默认值即"什么都没有"：不建视觉天空、不碰 IBL 句柄、不改 provider —— 与不调用
     * `SetupSkyEnvironment()` 完全等价（零隐式副作用）。
     */
    class SkyEnvironmentConfig
    {
    public:

        SkyMode mode = SkyMode::SHLightOnly;

        /**
         * 视觉天空的材质配方名（`GetRecipe()` 的名字，如 `IBLEnvironment.Sky`）。
         *
         * 空 = 不建视觉天空实体（`SHLightOnly` 的常见形态，也是"只想要天光"时的写法）。
         *
         * ⚠ 配方是**投影形态的变体入口**：Cube / Octahedral / Lambert 各自一份配方，
         *   它们只差 `[[textures]]` 的 `asset` 后缀；领域、表面模块、槽名全都相同。
         */
        AnsiString sky_recipe;

        /// 视觉天空几何体 URI；空 = 由 `mode` 自动选择（见 `GetSkyModeGeometryURI`）。
        AnsiString sky_geometry_uri;

        /// 天空实体世界缩放；0 = 用内置默认（全天球 1.0，半球穹顶 4.0）。
        float sky_scale = 0.0f;

        /// IBL 漫反射辐照度贴图（`.TexCube` / `.TexOct`，后缀自动判定投影与装载方式）。
        OSString irradiance_texture;

        /// IBL 镜面预滤波贴图（同上；mip 级数决定 `lod = roughness × (mip-1)` 的上界）。
        OSString prefilter_texture;

        /// BRDF LUT；空 = `res/image/brdflut.Tex2D`。
        OSString brdf_lut_texture;

        /**
         * SH9 天光系数（L2 辐照度表示，下标约定见 `AmbientEnvInfo::sh`，Z-up）。
         *
         * 只在 `sh_auto == false`（显式给系数）时被使用。`sh_auto == true` 时这组值被
         * 忽略，改由 `sh_params` 现算 —— 显式系数适合"已经离线算好"的场景。
         */
        Vector4f sh[9] = {};

        /**
         * 是否由 `EnvironmentSystem` 用**解析合成器**（`graph::ComputeSkySH9`）生成 SH9。
         *
         * 打开后，即使一张 IBL 贴图都没烘焙，场景也有与太阳方向物理一致的低频天光：
         * 天空被建模成「天顶→地平线线性渐变 + 下半球地面色 + 太阳方向辐照度」，
         * 其 SH 投影是闭式的（只有 idx0 / idx2 / idx6 + 太阳项的 9 项）。
         */
        bool sh_auto = true;

        /// `sh_auto` 为真时随主光源（光源行 0）变化重算 ⇒ 日落时天光自己变暖变暗。
        bool sh_track_sun = true;

        /// 解析合成参数。三段色的**亮度量级**、`exposure` 都在这里调。
        graph::SkySH9Params sh_params;

        /// 太阳辐照度标定：`sun_irradiance = 主光强度 × 本值`；0 = 天光不含太阳项。
        float sh_sun_irradiance_scale = 0.35f;

        SkyAmbientLDR ldr = SkyAmbientLDR::Auto;
        float ldr_exposure_base   = 2.5f;   ///< 基准曝光倍率
        float ldr_sun_scale       = 1.0f;   ///< 太阳高度角/光强调制权重
        float ldr_min_ambient     = 0.05f;  ///< 夜间保底环境光比例

        /// 天光 provider 模块名覆盖（如 `ambient_light_ibl`）；空 = 按 `mode` + 数据自动选择。
        AnsiString ambient_provider;

        /// 天空实体逐帧跟随主相机平移（无限远 / 无视差）。默认开。
        bool track_camera = true;

        /**
         * `[sun]` 默认太阳基准（schema 2 新增；schema-1 资产没有 ⇒ `sun_present == false`）。
         *
         * `sun_direction` 是 RH Z-up 世界系里**指向太阳**的单位向量（与光源行 0 的
         * forward 相反）。这里只是**纯数据承接**：把基准同步进 `LightSystem` 世界主光源
         * 行 0 是 `EnvironmentSystem` 的职责（见 todo-7 的 `[sun]` → 光源同步）。
         * 固定纹理类技术（Cube/Oct/Lambert）的太阳烘焙在贴图里，方向仅供天光/曝光调制
         * 参考 —— 图上太阳与光影不一致属于作者配置问题，引擎不做仲裁。
         */
        bool sun_present = false;
        Vector3f sun_direction = Vector3f(0.0f, 0.0f, 1.0f);
        Vector3f sun_color = Vector3f(1.0f, 1.0f, 1.0f);
        float sun_intensity = 1.0f;

    public:

        /// 是否要求建视觉天空实体。
        bool HasSkyVisual() const { return !sky_recipe.IsEmpty(); }

        /// 是否给了完整的 IBL 数据对（辐照度 + 预滤波）。
        bool HasIBLData() const
        {
            return !irradiance_texture.IsEmpty() && !prefilter_texture.IsEmpty();
        }

        /// SH 系数是否全 0（= 未显式提供）。
        bool IsSHZero() const;

        /// 该配置是否要产出 SH9 天光（`sh_auto` 或显式系数非零）。
        bool WantsSkySH() const { return sh_auto || !IsSHZero(); }
    };

    /**
     * 用环境资产规约（`*.env.toml`）**局部覆盖**一份配置。
     *
     * 覆盖语义完全由 `Presence<T>` 决定：文件里出现过的键写进来，没出现的键保留
     * `cfg` 的现值。因此传入一个缺省构造的 `SkyEnvironmentConfig` 得到的就是
     * 「文件写法 + 引擎缺省值」的合并结果；传入已有配置则等价于「在现有配置上打补丁」。
     *
     * `spec.name` / `spec.description` / `spec.schema` 不参与覆盖 —— 那是资产身份的
     * 元数据，不是运行时行为。`sh9.bands` 只在给了系数时整组覆盖（不做逐条合并），
     * 且 `sh9.auto_generate == true` 时下游会忽略它们。
     *
     * `technique` 指定**展开哪个技术段**（画质在资产外部选择的落点）：
     *   · `Default` —— 只走 schema-1 遗留镜像（注册表归一化时已把 `[base]` 与默认
     *     技术段镜像进 `[visual]/[ibl]/[ldr]/[ambient]`），行为与两参版本完全一致；
     *   · 显式档位 —— 先把技术相关字段**重置为引擎缺省**（换形态不留上档残留），
     *     再按 `[base]` → 该技术段的顺序覆盖，并把 `cfg.mode` 切到对应 `SkyMode`。
     * 资产里没配该技术段时得到一个"干净的空形态"（无 recipe / 无 IBL 贴图），
     * 由调用方决定回退 —— 数据层不猜。
     */
    void ApplyEnvironmentSpec(const graph::EnvironmentSpec &spec, SkyEnvironmentConfig &cfg,
                              SkyTechnique technique = SkyTechnique::Default);
}//namespace hgl::ecs
