#pragma once

#include<hgl/type/String.h>
#include<hgl/math/Vector.h>

namespace hgl::ecs
{
    using namespace hgl::math;

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

    /// 天空外观来源。只决定"默认用哪个几何体"，不决定天光算法。
    enum class SkyMode : unsigned char
    {
        /// 程序化假大气天空（`surface_sky_minimal` 解析大气，不吃贴图）+ 廉价天光。
        Atmosphere = 0,

        /// 全天球 + 立方体贴图背景（配方内 `asset = "cubefile:....TexCube"`）。
        CubeMapIBL,

        /// 全天球 + 八面体展开图背景（配方内 `asset = "....TexOct"`，仍是 2D 容器）。
        OctahedralIBL,

        /// 半球穹顶（`geo:builtin:dome_64`）+ Lambert Azimuthal 方位投影 2D 贴图，
        /// 顶点自带 UV（几何体 UV 即投影结果，材质侧纯 2D 采样）。
        LambertDome,

        /// 不建视觉天空实体（可选纯色/留空），只用 SH9 球谐做天光。
        SHLightOnly,
    };

    /// 模式名（诊断 / 日志用）。
    const char *GetSkyModeName(SkyMode mode) noexcept;

    /// 该模式的默认几何体 URI；`SHLightOnly` 没有默认几何体 ⇒ 返回 nullptr。
    const char *GetSkyModeGeometryURI(SkyMode mode) noexcept;

    /// 该模式的天空几何体是否自带 UV（半球穹顶 = 是，全天球 = 否）。
    /// 顶点流按此选择：带 UV 的几何体声明 `Position|TexCoord`，否则只声明 `Position`。
    bool SkyModeGeometryHasUV(SkyMode mode) noexcept;

    /**
     * LDR 曝光调制策略。
     *
     * IBL 天光对 **HDR 烘焙图**来说是物理正确的亮度；同一批数据存成 LDR（unorm）后
     * 少了高光动态范围，场景会明显偏暗。所以"是否提亮"必须由**数据本身的动态范围**
     * 决定，而不是全局开关。
     */
    enum class SkyAmbientLDR : unsigned char
    {
        /// 按实际装载出来的贴图格式判定：浮点格式 → 关闭；unorm 格式 → 开启。
        Auto = 0,
        /// 强制 HDR 语义（`ldr_param.x = 0`），不做任何提亮。
        Off,
        /// 强制 LDR 曝光调制（太阳高度角联动）。
        On,
    };

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
         * 全 0 且最终选择的是 SH 路径时，`SetupSkyEnvironment()` 会填一组**解析天空梯度**
         * 系数（上冷亮、下暗的默认天光）——保证"不烘焙任何贴图也能出图"，而不是黑球。
         */
        Vector4f sh[9] = {};

        SkyAmbientLDR ldr = SkyAmbientLDR::Auto;
        float ldr_exposure_base   = 2.5f;   ///< 基准曝光倍率
        float ldr_sun_scale       = 1.0f;   ///< 太阳高度角/光强调制权重
        float ldr_min_ambient     = 0.05f;  ///< 夜间保底环境光比例

        /// 天光 provider 模块名覆盖（如 `ambient_light_ibl`）；空 = 按 `mode` + 数据自动选择。
        AnsiString ambient_provider;

        /// 天空实体逐帧跟随主相机平移（无限远 / 无视差）。默认开。
        bool track_camera = true;

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
    };
}//namespace hgl::ecs
