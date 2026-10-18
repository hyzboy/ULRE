#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/type/String.h>

#include<vector>

namespace hgl::graph
{
    class BufferManager;
    class IGPUBuffer;

    template<typename T> class StructView;

    /**
     * EnvironmentManager - 环境综合信息统一管理器
     *
     * 集中持有所有环境 Profile（数据 + GPU 物化），设备级唯一（GraphicsContext 模块）。
     *
     * ⚠ 归属订正（2026-09-28；决策见 doc/world-addresses-and-camera-model-plan.md §4）：
     * **Env 随世界** —— profile 的所有权与生命周期跟随世界（世界创建时选/建自己的
     * profile、销毁时 `Release()` 归还；内置 default 可共享），参数段的**地址发布按世界**
     * （写世界表 `WorldAddresses`，由 `ECSContext::SyncWorldAddresses`
     * 每帧写本世界槽）。C2 之前地址走全局字段 ⇒ 多世界同帧只有最后解析的那个 profile 生效（互踩）。
     *
     * ⚠ T6b/P4：**影子数据（`ShadowInfo`）已迁出本模块** —— 它随世界（`ECSContext`），
     * 世界私有 SSBO 由 `ECSContext::InitializeShadowInfoTable` 建、`CommitShadowInfo` 写本帧槽，
     * 地址经世界表 `addr_shadow` 下发。本模块管**两个 provider 槽的参数块物化**
     *（`ambient_env_ubo`（`ambient_light_provider`）
     * + profile 生命周期）。
     *
     * 分层约定：
     * - 数据层：EnvironmentInfo（纯数据，CPU 侧唯一权威在 Profile::cpu）
     * - 管理层：本类（Profile 注册 / GPU UBO 物化 / 脏标记 / **按 profile 取设备地址**）
     * - 选择层：**世界**（`ECSContext::GetEnvProfileID()`；世界初始化时取本世界 RT 的 env_profile）
     * - 绑定层：**世界表**（`WorldAddresses::addr_ambient_env`；Scene 集已随 S3 整体退场，
     *   参数段全部经 pc_root 下的表 + BDA 解引用，无 binding 可推）
     *
     * ⚠ **2026-10-14：ambient provider 模块的选择从材质 TOML 上移到本层（Profile）**。
     * 环境光有**两条正交的轴**，必须由同一个所有者配对，否则靠人保证一致：
     *   · 数据轴 = `Profile::cpu.ambient_env`（IBL 句柄，物化成 UBO 走世界表）；
     *   · 算法轴 = `Profile::ambient_module_name`（GLSL 模块名，决定 program 身份）。
     * 旧实现把算法轴放在材质（`ambient_module_name` TOML 键）⇒ 同一套 PBR 表面被迫
     * 复制成两份材质（`Lit` / `LitIBL`）、且"材质选 IBL 但环境没给句柄"会静默发黑。
     * 现在：**环境（世界）决定算法 + 数据，材质只声明表面**。
     * `ambient_module_name` 为空 = 用场景渲染模板的默认 provider（`indirect_sky_ambient`）。
     *
     * GPU 上传统一走设备级 dirty 扫描（RenderBufferUploadSystem），
     * default Profile 在 GraphicsContext 初始化阶段即物化并标脏，
     * 保证任何 world（含离屏 RenderOnce）第一帧拿到的就是有效参数数据。
     */
    GRAPH_MODULE_CLASS(EnvironmentManager)
    {
    public:

        struct Profile
        {
            EnvProfileID    id = kEnvProfileInvalid;
            AnsiString      name;
            EnvironmentInfo cpu;                                               ///< CPU 侧唯一权威数据
            StructView<AmbientEnvInfo> *ambient_env_ubo = nullptr;  ///< ambient_env 段 GPU 物化（同上）

            /// `ambient_light_provider` 槽用哪个 GLSL 模块（**算法轴**，CPU 侧，不入 GPU）。
            /// 空 = 用场景渲染模板的默认 provider。与 `cpu.ambient_env`（**数据轴**）配对，
            /// 二者必须一起换：选 IBL 模块却没给句柄 ⇒ 采样句柄 0 ⇒ 静默发黑。
            AnsiString      ambient_module_name;
        };

    private:

        EnvProfileID next_id = kEnvProfileDefault + 1;
        std::vector<Profile *> profiles;

        Profile *FindProfile(EnvProfileID id) const;
        Profile *FindProfile(const AnsiString &name) const;

        bool MaterializeAmbientEnvUBO(Profile *profile);

        void EnsureDefault();

    public:

        EnvironmentManager(GraphicsContext *gc);
        virtual ~EnvironmentManager();

        void OnGraphicsContextChanged(GraphicsContext *gc) override;
        void Release() override;

        /// 注册新环境 Profile（name 重复时返回已有句柄）
        EnvProfileID Create(const AnsiString &name, const EnvironmentInfo &init_info = {});
        EnvProfileID Find(const AnsiString &name) const;

        /// 编辑 Profile 数据（返回 CPU 权威数据指针；句柄无效返回 nullptr）
        EnvironmentInfo *Edit(EnvProfileID id);
        const EnvironmentInfo *Get(EnvProfileID id) const;

        /// 数据改完调用：ambient_env 立即写入它的单份 UBO（影子数据不在本模块，见类注释）。
        void MarkDirty(EnvProfileID id);

        /// **算法轴**：指定该 profile 用哪个 `ambient_light_provider` 模块（注册名，
        /// 与 `@ulre name` 一致）。空串 = 回到场景渲染模板默认 provider。
        /// 只改选择，不动 GPU 数据 ⇒ 无需 `MarkDirty`；下次解析材质程序时
        /// program 身份（`HashMaterialProgramBuildContext` 含本维度）自然变化。
        /// 句柄无效（或 `kEnvProfileInvalid`）⇒ 返回 false 且不改任何东西。
        bool SetAmbientProvider(EnvProfileID id, const AnsiString &module_name);

        /// 取该 profile 的 ambient provider 模块名（**引用**，有效期到下次
        /// `SetAmbientProvider` / `Release` 为止）。无 profile / 未设置 ⇒ 空串。
        const AnsiString &GetAmbientProvider(EnvProfileID id) const;

        /// 世界表用：本 profile 的 ambient_env 段**设备地址**（懒物化）。取不到返回 0 并报错 ——
        /// 同取址纪律（0 地址会让读该段的 shader 解引用 0 基址 ⇒ 静默发黑）。
        uint64_t GetAmbientEnvAddress(EnvProfileID id);

        /// 归还 profile：释放它的参数段 GPU 物化并从注册表移除。
        /// **世界销毁时归还自己创建/拥有的 profile**；`kEnvProfileDefault` 与无效句柄是 no-op
        /// （内置 default 可共享）。归还后再用该 id 会回退 default（`GetAmbientEnvAddress` 语义同 `Get`）。
        void Release(EnvProfileID id);
    };//class EnvironmentManager
}//namespace hgl::graph
