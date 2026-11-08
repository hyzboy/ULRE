#pragma once

/**
 * 环境资产规约（`*.env` 二进制 / `*.env.toml` 文本的字段契约真源）。
 *
 * 一份环境资产 = 「画什么天」+「用什么天光」。这两件事在引擎里是**正交**的，
 * 所以本规约也按正交的字段分组描述，而不是做成一张「模式 → 全部行为」的大表：
 *
 *   [visual]  —— 视觉天空：几何体 + 材质配方 + 缩放 + 是否跟随相机
 *   [ambient] —— 天光来源：模式（只决定"默认几何体/默认 provider"）+ provider 覆盖
 *   [ibl]     —— 烘焙贴图：辐照度 / 预滤波 / BRDF LUT
 *   [ldr]     —— LDR 曝光调制（同一批数据存成 unorm 后会明显偏暗）
 *   [sh9]     —— 球谐天光：解析合成开关或显式系数
 *
 * ```toml
 * schema = 1
 * name = "Sky34.CubeIBL"
 * description = "sky_34 立方体贴图 + 同一批 IBL 天光"
 *
 * [visual]
 * recipe = "IBLEnvironment.Sky"          # 空 ⇒ 不建视觉天空实体
 * scale  = 1.0                            # 0 ⇒ 用模式内置默认
 *
 * [ambient]
 * mode = "cubemap_ibl"
 *
 * [ibl]
 * irradiance = "texcube:pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k"
 * prefilter  = "texcube:pack(res):image/Environments/sky_34_2k/sky_34_prefilter_2k"
 *
 * [ldr]
 * mode = "auto"
 *
 * [sh9]
 * auto = true
 * ```
 *
 * ── schema 2（统一天空资产）─────────────────────────────────────────────────
 *
 * 一份资产把**每种天空技术**各自需要的配置并列写全，`[ambient].mode` 只表示
 * 「默认技术」；画质档位/平台由**外部**选择技术（`SkyEnvironmentConfig::
 * ApplyTechnique()`），资产本身不含画质分级概念。
 *
 * ```toml
 * schema = 2
 * name = "Sky34"
 *
 * [sun]                    # 默认太阳（可选；固定纹理技术写了也不保证可动）
 * direction = [0.35, -0.55, 0.75]   # RH Z-up 世界单位方向
 * color     = [1.0, 0.98, 0.95]     # 线性 RGB
 * intensity = 40.0
 *
 * [base]                   # 各技术共用的视觉身份（可被技术段同名键覆盖）
 * geometry = "geo:builtin:skybox"
 * scale    = 1.0
 * track_camera = true
 *
 * [ambient]
 * mode = "cubemap_ibl"      # 默认技术
 *
 * [cubemap]                 # 立方体贴图 IBL
 * recipe    = "IBLEnvironment.Sky"
 * irradiance = "texcube:pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k"
 * prefilter  = "texcube:pack(res):image/Environments/sky_34_2k/sky_34_prefilter_2k"
 * ldr_mode  = "auto"
 *
 * [octahedral]              # 八面体 IBL（同一环境另一种打包）
 * recipe     = "OctEnvironment.Sky"
 * irradiance = "texoct:pack(res):image/Environments/sky_34_2k/sky_34_cubemap_2k"
 * prefilter  = "texoct:pack(res):image/Environments/sky_34_2k/sky_34_prefilter_2k"
 * ldr_mode   = "auto"
 *
 * [lambert_dome]
 * recipe     = "LambertDome.Sky"
 * geometry   = "geo:builtin:dome_64"
 * scale      = 4.0
 * irradiance = "tex2d:pack(res):image/Environments/sky_34_2k/sky_34_irradiance"
 * prefilter  = "tex2d:pack(res):image/Environments/sky_34_2k/sky_34_prefilter"
 *
 * [atmosphere]              # 解析大气（不吃贴图，只有配方）
 * recipe = "SkyMinimal.Sky"
 *
 * [sh9]                     # 全画质档共用的廉价天光源，保持顶层
 * auto = true
 * track_sun = true
 * ```
 *
 * 同一份数据在不同画质档的用法：高配 `[cubemap]`（HDR IBL）、低配
 * `[octahedral]`/`[lambert_dome]`（LDR IBL）、超低配 `[sh9]`（SHLightOnly）。
 * schema 1 文件仍可直接装载：注册时 `NormalizeEnvironmentSpec()` 把旧
 * `[visual]/[ibl]/[ldr]` 中**缺失的键**镜像进 `[base]` 与默认技术段（只填不清，
 * 旧字段原样保留，两套读取路径长期共存）。
 *
 * ── 为什么字段几乎全是 `hgl::serialize::Presence<T>` ─────────────────────────
 *
 * 环境资产是**局部覆盖**语义：文件里只写和 `hgl::ecs::SkyEnvironmentConfig`
 * 缺省值不同的键。`Archive` 的缺键行为是「保留类内缺省值」，取值问题由此解决；
 * 但 `false` / `0` / `"Auto"` 都是合法取值，光看值分不清「写成了缺省值」和
 * 「根本没写」。`Presence<T>` 把「这个键出现过」单独记下来，于是：
 *
 *   · 缺键           → `present == false`，下游保留类内缺省值
 *   · 显式写成缺省值 → `present == true`，覆盖成缺省值
 *   · 显式写成别的值 → `present == true`，覆盖成该值
 *
 * 生成器：`src/Tools/AssetGen/asset_gen.py`（`[group.Environment]`）。
 * 运行时落地：`hgl::ecs::EnvironmentSystem::SetupSkyEnvironment()`。
 */

#include <hgl/asset/AssetAnnotation.h>
#include <hgl/CoreType.h>
#include <hgl/math/VectorTypes.h>
#include <hgl/serialize/Archive.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    /**
     * 天空外观来源。只决定"默认用哪个几何体"与"默认用哪个天光 provider"，
     * 不决定天光数据的来源 —— 两者可以任意组合（例如"八面体背景 + SH9 天光"）。
     *
     * ⚠ 换**贴图投影形态**必须换**配方**（recipe 变体），不能在运行时改写配方里的
     *   `[[textures]] asset`：配方句柄与 `resource_id` 在装载时焊死。投影实现本身由
     *   C++ 侧按资源后缀选择（`.TexCube` / `.TexOct` / 其余 2D），主着色器与表面模块
     *   没有任何分支。
     */
    enum class SkyMode : uint8
    {
        /// 程序化假大气天空（`surface_sky_minimal` 解析大气，不吃贴图）+ 廉价天光。
        Atmosphere = 0,

        /// 全天球 + 立方体贴图背景（配方内 `asset = "....TexCube"`）。
        CubeMapIBL,

        /// 全天球 + 八面体展开图背景（配方内 `asset = "....TexOct"`，仍是 2D 容器）。
        OctahedralIBL,

        /// 半球穹顶（`geo:builtin:dome_64`）+ Lambert Azimuthal 方位投影 2D 贴图，
        /// 顶点自带 UV（几何体 UV 即投影结果，材质侧纯 2D 采样）。
        LambertDome,

        /// 不建视觉天空实体（可选纯色/留空），只用 SH9 球谐做天光。
        SHLightOnly,

        /// **什么都没有**：不建视觉天空、不喂 IBL / SH 数据，天光 provider 钉到
        /// `ambient_light_none`（Null Object，恒零）。
        ///
        /// 存在的意义是让"没有环境"成为一个**可以写下来的显式配置**，而不是靠
        /// "不调用 `SetupSkyEnvironment()`"来表达。前向材质仍然有 `ambient_light_provider`
        /// 槽参与编译，光照累加式一行不变，没有 `#if`。
        None,
    };

    /**
     * LDR 曝光调制策略。
     *
     * IBL 天光对 **HDR 烘焙图**来说是物理正确的亮度；同一批数据存成 LDR（unorm）后
     * 少了高光动态范围，场景会明显偏暗。所以"是否提亮"必须由**数据本身的动态范围**
     * 决定，而不是全局开关。
     */
    enum class SkyAmbientLDR : uint8
    {
        /// 按实际装载出来的贴图格式判定：浮点格式 → 关闭；unorm 格式 → 开启。
        Auto = 0,
        /// 强制 HDR 语义（`ldr_param.x = 0`），不做任何提亮。
        Off,
        /// 强制 LDR 曝光调制（太阳高度角联动）。
        On,
    };

    /// `[visual]` 视觉天空。
    struct HGL_ASSET_STRUCT EnvironmentVisualSpec
    {
        /**
         * 天空材质配方名（`GetRecipe()` 的名字，如 `IBLEnvironment.Sky`）。
         *
         * 空 ⇒ 不建视觉天空实体（`sh9_only` 的常见形态，也是"只想要天光"时的写法）。
         */
        hgl::serialize::Presence<AnsiString> recipe;

        /// 天空几何体 URI；空 ⇒ 由 `ambient.mode` 自动选择。
        hgl::serialize::Presence<AnsiString> geometry;

        /// 天空实体世界缩放；0 ⇒ 用内置默认（全天球 1.0，半球穹顶 4.0）。
        hgl::serialize::Presence<float> scale;

        /// 天空实体逐帧跟随主相机平移（无限远 / 无视差）。
        hgl::serialize::Presence<bool> track_camera;
    };

    /// `[ambient]` 天光来源。
    struct HGL_ASSET_STRUCT EnvironmentAmbientSpec
    {
        /// 天空外观模式。缺键取 `SkyMode::SHLightOnly`（与 `SkyEnvironmentConfig` 缺省一致）。
        hgl::serialize::Presence<SkyMode> mode;

        /// 天光 provider 模块名覆盖（如 `ambient_light_ibl`）；空 ⇒ 按 mode + 数据自动选择。
        hgl::serialize::Presence<AnsiString> provider;
    };

    /// `[ibl]` 烘焙贴图。后缀决定投影与装载方式（`.TexCube` 立方体 / `.TexOct` 八面体 / 其余 2D）。
    struct HGL_ASSET_STRUCT EnvironmentIBLSpec
    {
        /// 漫反射辐照度贴图。
        hgl::serialize::Presence<OSString> irradiance;

        /// 镜面预滤波贴图；mip 级数决定 `lod = roughness × (mip-1)` 的上界。
        hgl::serialize::Presence<OSString> prefilter;

        /// BRDF LUT；空 ⇒ `res/image/brdflut.Tex2D`。
        hgl::serialize::Presence<OSString> brdf_lut;
    };

    /// `[ldr]` LDR 曝光调制。
    struct HGL_ASSET_STRUCT EnvironmentLDRSpec
    {
        hgl::serialize::Presence<SkyAmbientLDR> mode;

        /// 基准曝光倍率。
        hgl::serialize::Presence<float> exposure_base;

        /// 太阳高度角/光强调制权重。
        hgl::serialize::Presence<float> sun_scale;

        /// 夜间保底环境光比例。
        hgl::serialize::Presence<float> min_ambient;
    };

    /**
     * `[[sh9.bands]]` 中的一条 L2 球谐系数（线性辐亮度，`.a` 不参与计算故不存）。
     *
     * 数组顺序即基函数顺序（Z-up，与 `ambient_light_sh.glsl` 逐项一致）：
     *
     * ```
     * [0] Y₀₀ ∝ 1            [3] Y₁₁ ∝ x            [6] Y₂₀ ∝ 3z²-1
     * [1] Y₁₋₁ ∝ y           [4] Y₂₋₂ ∝ xy          [7] Y₂₁ ∝ xz
     * [2] Y₁₀ ∝ z           [5] Y₂₋₁ ∝ yz          [8] Y₂₂ ∝ x²-y²
     * ```
     */
    struct HGL_ASSET_STRUCT EnvironmentSH9BandSpec
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
    };

    /**
     * `[sh9]` 球谐天光。
     *
     * 两条互斥的来源：
     *
     *   · `auto = true`  ⇒ 由 `graph::ComputeSkySH9()` 按**主光源**现算（日落时天光
     *     自己变暖变暗），`bands` 被忽略；
     *   · `auto = false` ⇒ 用 `bands` 里离线算好的系数（必须正好 9 条）。
     *
     * ⚠ 解析合成器的三段渐变色（天顶/地平线/地面）由 `EvaluateSkyGradientForSun()`
     *   按太阳仰角插值，**不在**资产里开放 —— 那是"跟着太阳走"的实现细节，
     *   写进文件只会得到一个永远被覆盖的键。
     */
    struct HGL_ASSET_STRUCT EnvironmentSH9Spec
    {
        /// 是否由引擎用解析合成器生成 SH9。
        hgl::serialize::Presence<bool> auto_generate;

        /// `auto_generate` 为真时随主光源变化重算。
        hgl::serialize::Presence<bool> track_sun;

        /// 太阳辐照度标定：`sun_irradiance = 主光强度 × 本值`；0 = 天光不含太阳项。
        hgl::serialize::Presence<float> sun_irradiance_scale;

        /// 整体倍率（线性），用于把美术给的色标定到渲染管线的亮度量级。
        hgl::serialize::Presence<float> exposure;

        /// 显式系数（`auto_generate == false` 时使用），顺序见 `EnvironmentSH9BandSpec`。
        hgl::serialize::Presence<ValueArray<EnvironmentSH9BandSpec>> bands;
    };

    /// `[sun]` 默认太阳（schema 2）。
    struct HGL_ASSET_STRUCT EnvironmentSunSpec
    {
        /// 世界空间单位方向（RH Z-up，指向太阳）；同步给 LightSystem 主光行。
        hgl::serialize::Presence<hgl::math::Vector3f> direction;

        /// 太阳线性 RGB 颜色。
        hgl::serialize::Presence<hgl::math::Vector3f> color;

        /// 太阳强度（与主光源辐照度联动的标定值）。
        hgl::serialize::Presence<float> intensity;
    };

    /// `[base]` 各技术共用的视觉身份（schema 2；可被技术段同名键覆盖）。
    struct HGL_ASSET_STRUCT EnvironmentBaseSpec
    {
        /// 天空几何体 URI；空 ⇒ 由技术各自内置。
        hgl::serialize::Presence<AnsiString> geometry;

        /// 天空实体世界缩放；0 ⇒ 用技术内置默认。
        hgl::serialize::Presence<float> scale;

        /// 天空实体逐帧跟随主相机平移。
        hgl::serialize::Presence<bool> track_camera;
    };

    /**
     * 一种天空技术的完整配置段（schema 2）。
     *
     * `[cubemap]` / `[octahedral]` / `[lambert_dome]` / `[atmosphere]` 共用此形状：
     *   · `[atmosphere]` 只用 `recipe`（解析大气不吃贴图，IBL/LDR 键写了也无意义）；
     *   · `[lambert_dome]` 的 `geometry/scale` 通常覆盖 `[base]`（穹顶 ≠ 全天球）；
     *   · `recipe` 的空 ⇒ 该档**不建**视觉天空（如只用 IBL 贴图喂光照的档位）；
     *   · LDR 键是该档烘焙图的曝光调制（同一环境 HDR/LDR 打包各配各的）。
     */
    struct HGL_ASSET_STRUCT EnvironmentTechniqueSpec
    {
        /// 该技术的天空材质配方名。
        hgl::serialize::Presence<AnsiString> recipe;

        /// 该技术的几何体覆盖（空 ⇒ 取 `[base].geometry`）。
        hgl::serialize::Presence<AnsiString> geometry;

        /// 该技术的缩放覆盖（0 ⇒ 取 `[base].scale`）。
        hgl::serialize::Presence<float> scale;

        /// 漫反射辐照度贴图 URI（`tex2d:/texcube:/texoct:` 前缀区分投影）。
        hgl::serialize::Presence<OSString> irradiance;

        /// 镜面预滤波贴图 URI。
        hgl::serialize::Presence<OSString> prefilter;

        /// BRDF LUT；空 ⇒ `tex2d:pack(res):image/brdflut`。
        hgl::serialize::Presence<OSString> brdf_lut;

        /// 该档烘焙图的 LDR 曝光调制策略。
        hgl::serialize::Presence<SkyAmbientLDR> ldr_mode;

        /// 基准曝光倍率。
        hgl::serialize::Presence<float> ldr_exposure_base;

        /// 太阳高度角/光强调制权重。
        hgl::serialize::Presence<float> ldr_sun_scale;

        /// 夜间保底环境光比例。
        hgl::serialize::Presence<float> ldr_min_ambient;

        /// 该技术的天光 provider 覆盖（空 ⇒ 取 `[ambient].provider`）。
        hgl::serialize::Presence<AnsiString> provider;
    };

    /// 一份环境资产文件的完整字段规约。
    struct HGL_ASSET_STRUCT EnvironmentSpec
    {
        /// 版本号。接受 1（旧单技术形态）与 2（统一多技术形态），判断留在解析层。
        uint32 schema = 1;

        /// 资产名（`env:@Name` 按名引用时的键）。
        AnsiString name;

        AnsiString description;

        /* ── schema 1 遗留字段（长期保留；装载时由 NormalizeEnvironmentSpec
           镜像进 [base]/默认技术段，只填不清，两套读取路径共存）────────── */

        EnvironmentVisualSpec  visual;
        EnvironmentAmbientSpec ambient;
        EnvironmentIBLSpec     ibl;
        EnvironmentLDRSpec     ldr;
        EnvironmentSH9Spec     sh9;

        /* ── schema 2 新增段 ──────────────────────────────────────────────── */

        /// `[sun]` 默认太阳与时间（可选）。
        hgl::serialize::Presence<EnvironmentSunSpec> sun;

        /// `[base]` 共用视觉身份。
        hgl::serialize::Presence<EnvironmentBaseSpec> base;

        /// `[cubemap]` 立方体贴图 IBL 档。
        hgl::serialize::Presence<EnvironmentTechniqueSpec> cubemap;

        /// `[octahedral]` 八面体 IBL 档。
        hgl::serialize::Presence<EnvironmentTechniqueSpec> octahedral;

        /// `[lambert_dome]` Lambert 穹顶档。
        hgl::serialize::Presence<EnvironmentTechniqueSpec> lambert_dome;

        /// `[atmosphere]` 解析大气档。
        hgl::serialize::Presence<EnvironmentTechniqueSpec> atmosphere;
    };

    /**
     * 把 schema 1 的 `[visual]/[ibl]/[ldr]/[ambient]` **镜像**进 `[base]` 与
     * `[ambient].mode` 指向的技术段（只填缺失键，绝不清空旧字段）。
     *
     * 注册路径装载后自动调用；手写 schema 2 文件调用它也无害（幂等：
     * 已有键优先）。schema 2 的 SHLightOnly/None 默认技术没有视觉段，只补 `[base]`。
     */
    void NormalizeEnvironmentSpec(EnvironmentSpec &spec);

    /// 取某技术对应的配置段（schema 2）；未配置返回 nullptr。
    const EnvironmentTechniqueSpec *FindTechniqueSection(const EnvironmentSpec &spec, SkyMode mode);
}//namespace hgl::graph
