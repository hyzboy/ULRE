#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/ecs/systems/render/SkyEnvironmentConfig.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/EnvironmentAssetRegistry.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/type/String.h>
#include<memory>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
        class EnvironmentManager;
        class GraphicsContext;
        class Texture;
    }

    namespace ecs
    {
        /**
         * EnvironmentSystem
         *
         * 世界环境（**天空 / IBL**）编辑入口（瘦转发层）。
         * 环境数据与 GPU UBO 统一归 EnvironmentManager（GraphicsContext 模块）所有；
         * 本系统只按所属 world 的 Profile 转发编辑请求，不拥有任何 GPU 资源。
         *
         * 拆域结果（T6a + T6b）：级联阴影托管在 `MainLightShadowSystem`；**光源参数**
         *   （方向 / 颜色 / 强度 / 角径 / 阴影开关）在世界私有**光源行**（`LightSystem` /
         *   `WorldAddresses.addr_lights`）—— 本系统只剩**一个 provider 槽参数块**：
         *   `ambient_env`（环境光 / IBL 句柄）。
         *   ⚠ **2026-10-07：`sky` 段（原 `SkyInfo`）整删**（两个字段全仓无写者，取值搬成
         *   `sky_atmosphere.glsl` 内常量；`addr_sky` 同步退出世界表）⇒ 天空外观的编辑入口不复存在。
         *   见 `doc/future/lighting-shadow-domain-split-plan.md` §3。
         */
        class EnvironmentSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;

            // ── 天空实体托管的运行时状态（`SetupSkyEnvironment` 写入）──
            //
            // `GeometryData::SetPrimitiveAsset()` 存的是**指针**（不拷贝），所以
            // `PrimitiveAsset` 必须由本系统持有；而它的 `variants_` 又指向
            // `sky_variant`，两者都得活到实体销毁。
            EntityID  sky_entity_id = EntityID::Invalid();

            graph::PrimitiveVariant sky_variant{};
            graph::PrimitiveAsset   sky_primitive{};

            /// 非拥有：由 `TextureManager` 持有，这里只借来看格式 / mip 级数（阶段 3 复用）。
            const graph::Texture *sky_irradiance_tex = nullptr;
            const graph::Texture *sky_prefilter_tex  = nullptr;

            /// 本次托管的完整配置（阶段 3 的太阳耦合 / SH 重算要回读）。
            SkyEnvironmentConfig sky_config;

            /// `env:` URI 的解析缓存（`SetupSkyEnvironment(uri)` 用；`env:@Name` 依赖它）。
            /// 生命周期跟随本系统，因此同一份资产在反复热切之间只解析一次。
            graph::EnvironmentAssetRegistry environment_registry;

            graph::EnvProfileID ResolveProfileID() const;
            graph::EnvironmentManager *ResolveManager();
            graph::GraphicsContext *ResolveGraphicsContext();

            /// 装载单张贴图并按投影形态注册 bindless；失败返回 0。
            /// `mip_out` 回填实际 mip 级数，`tex_out` 回填非拥有的纹理指针（都可为空）。
            uint32 LoadSkyTexture(const OSString &uri, uint32 *mip_out, const graph::Texture **tex_out);

            /// 上一次参与 SH 合成的太阳快照（方向 / 颜色 / 强度）：三者都没变就不重算、不标脏。
            math::Vector4f sky_last_sun_dir = math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            math::Vector4f sky_last_sun_color = math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f);
            float sky_last_sun_intensity = 0.0f;

            /// 是否已经写过一次 SH（首次必须写；之后只在太阳动了才写）。
            bool sky_sh_written = false;

            /// 是否处于托管状态（可能没有视觉天空实体：`SHLightOnly` 只托管天光）。
            bool sky_managed = false;

            /// 读主光源行 0 + `sky_config.sh_params` 重算 9 个 SH 系数并写入 `ambient_env`。
            /// 没有 `ambient_env` 参数块时返回 false（未做任何修改）。
            bool WriteSkySHFromMainLight();

        public:

            EnvironmentSystem(const std::string &name = "EnvironmentSystem");
            ~EnvironmentSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }

            // 环境光 provider 槽（`ambient_light_provider`）的参数块。
            // 修改后需调用 MarkAmbientEnvDirty()。
            graph::AmbientEnvInfo *EditAmbientEnvInfo();
            // 2026-10-10（审计 §1.1）：`GetAmbientEnvInfo()` / `SetAmbientEnvInfo()` 全仓零调用者 ⇒ 删除
            //（读写都走 `EditAmbientEnvInfo()` + `MarkAmbientEnvDirty()`）。
            void MarkAmbientEnvDirty();

            /**
             * 配置 LDR 环境光曝光与太阳光强度动态调制（仅对 LDR 生效）
             * @param enable 是否启用 LDR 曝光调制
             * @param base_exposure 基准曝光倍率（默认 2.5f）
             * @param sun_scale 太阳高度角与光强调制权重（默认 1.0f）
             * @param min_ambient 夜间 / 太阳落山后的最低环境光保底比例（默认 0.05f）
             */
            void SetLDRExposure(bool enable, float base_exposure = 2.5f, float sun_scale = 1.0f, float min_ambient = 0.05f);

            // ── ambient provider **模块选择**（2026-10-14：从材质 TOML 上移到环境层）──
            //
            // 环境光有两条正交的轴，本系统是它们的**作者侧统一入口**：
            //   · 数据轴 = `EditAmbientEnvInfo()` + `MarkAmbientEnvDirty()`（IBL 句柄）；
            //   · 算法轴 = `SetAmbientProvider()`（用哪个 GLSL 模块，如 `indirect_ibl`）。
            // 材质只声明表面（`material_source_module_name` / `ntb_module_name`），
            // 不再声明 ambient ⇒ 同一套 PBR 材质在任何环境下都能用。
            //
            // `module_name` = GlslModuleRegistry 注册名（`@ulre name`）。
            // 空串 = 回到场景渲染模板默认 provider（`indirect_sky_ambient`）。
            // 返回 false = 环境管理不可用 / profile 无效（未做任何修改）。
            // ⚠ 换模块**必须**同时确认 `ambient_env` 已给有效句柄（IBL 模块采样它）：
            //   选了 IBL 却没给句柄 ⇒ 采样 0 句柄 ⇒ 静默发黑。
            bool SetAmbientProvider(const AnsiString &module_name);
            /// 当前生效的 ambient provider 模块名（空 = 模板默认）。
            const AnsiString &GetAmbientProvider() const;

            // ── 天空 + 天光**统一托管**（数据轴 + 算法轴的组合入口）──
            //
            // 一次调用落成三件事：
            //   1. 视觉天空实体：按 `mode` 选几何体（全天球 / 半球穹顶）、按 `sky_recipe`
            //      选配方（Cube / Octahedral / 2D / 解析大气 —— 配方是投影变体的入口）；
            //   2. 天光数据：按 URI 后缀（`.TexCube` / `.TexOct` / 其余）分别走
            //      `LoadTextureCube` / `LoadTexture2D`，注册 bindless 后写 `AmbientEnvInfo`
            //      （`w` = prefilter 的 mip 级数）；SH 系数直接写 `sh[9]`；
            //   3. 天光算法：`SetAmbientProvider()` 选模块，并把 LDR/HDR 判定落到 `ldr_param`。
            //
            // 「视觉天空」与「天光算法」**解耦**：可以八面体天空配 SH 天光，也可以半球
            //  Lambert 背景配烘焙好的 IBL 天光 —— 两者互不约束。
            //
            // 重复调用等价于「先 `DestroySkyEnvironment()` 再建」，便于运行期热切环境。
            // 失败时不留下半个状态（要么全套就位，要么回到未托管）。
            bool SetupSkyEnvironment(const SkyEnvironmentConfig &config);

            /**
             * 用 `env:` URI 建立天空 + 天光。
             *
             * ```
             * env:file:res/env/sky_34.env.toml   落盘文件
             * env:@Sky34                         表内名字（此前 `RegisterFromToml` 载入过）
             * res/env/sky_34.env.toml            省略类型段，按文件处理
             * ```
             *
             * 内部只做三件事：查表（带缓存）→ `ApplyEnvironmentSpec()` 映射成
             * `SkyEnvironmentConfig` → 调上面的重载。因此语义、日志、失败清理与直接
             * 传配置**完全一致**，URI 只是一个可序列化的入口。
             *
             * 失败（文件不存在 / 解析失败 / 校验不过）时不改变现有环境，返回 false。
             */
            bool SetupSkyEnvironment(const OSString &env_uri);

            /**
             * 预载一个目录里的所有 `.env.toml`（不递归），使 `env:@<name>` 可用。
             *
             * `env:@Name` 只能命中**已注册**条目，而名字写在文件里、无法从 URI 反推路径。
             * 因此「按名字引用」需要先有这一次目录入口（通常开机做一次）：
             *
             * ```cpp
             * env->LoadEnvironmentDirectory(OS_TEXT("ShaderLibrary/asset/environment"));
             * env->SetupSkyEnvironment(OS_TEXT("env:@Sky34.CubeMapIBL"));
             * ```
             *
             * 目录不存在返回 false；单文件失败只计数不影响其它文件。
             */
            bool LoadEnvironmentDirectory(const OSString &directory,
                                          int *out_file_count = nullptr,
                                          int *out_error_count = nullptr);

            /// 本系统持有的 `env:` URI 资产表（诊断 / 外部注册入口）。
            graph::EnvironmentAssetRegistry &GetEnvironmentRegistry()
            {
                return environment_registry;
            }

            /// 反向清理：销毁天空实体、把 `ambient_env` 清零、provider 回到模板默认。
            void DestroySkyEnvironment();

            /// 是否处于天空托管状态（`SHLightOnly` 只有天光、没有视觉天空实体，同样算托管）。
            bool HasSkyEnvironment() const { return sky_managed; }

            /// 天空实体 ID（未托管 = `EntityID::Invalid()`）。
            EntityID GetSkyEntityID() const { return sky_entity_id; }

            /**
             * 写入一组**解析天空梯度** SH9 系数（上冷亮、下暗），仅供没有烘焙数据时兜底。
             *
             * 走 `graph::ComputeSkySH9()`：天空被建模成「天顶→地平线线性渐变 +
             * 下半球地面色 + 太阳方向辐照度」，取闭式 SH 投影。太阳方向 / 颜色 / 强度
             * 取自光源行 0，所以结果与当前光照一致（不是一组写死的常数）。
             */
            void SetDefaultSkyGradientSH();

            /**
             * 按当前主光源重算 SH9 天光（`sh_auto` 且 `sh_track_sun` 时由 `Update()` 调）。
             *
             * 太阳方向变化小于阈值时不重算、不标脏（静态光场景零开销）。
             * @return 是否真的重写了系数
             */
            bool UpdateSkySHFromSun();

            /// 逐帧把天空实体的世界位置钉到主相机（`track_camera` 生效时才动）。
            void Update(float delta_time) override;
        };
    }//namespace ecs
}//namespace hgl
