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
     * **Env 随世界** —— profile 的所有权与生命周期最终跟随世界（世界创建时选/建自己的
     * profile、销毁时归还；内置 default 可共享），且 sky / shadow / env 的**地址发布必须按世界**
     * （写世界表 `WorldAddresses`）。现状"RT/WORLD 只持 EnvProfileID 引用 + 地址写全局字段"
     * 在多世界同帧下只有最后解析的那个 profile 生效（互踩）。
     *
     * 分层约定：
     * - 数据层：EnvironmentInfo（纯数据，CPU 侧唯一权威在 Profile::cpu）
     * - 管理层：本类（Profile 注册 / GPU UBO 物化 / 脏标记）
     * - 选择层：世界的 RT → EnvProfileID（未设置 = kEnvProfileDefault）
     * - 绑定层：RenderSceneUBOSystem 解析 GetSkyUBO()/GetShadowUBO() 后写地址表
     *  （世界表落地前为全局表 `GlobalAddresses`；Scene 集已随 S3 整体退场，不再有 binding）
     *
     * GPU 上传统一走设备级 dirty 扫描（RenderBufferUploadSystem），
     * default Profile 在 GraphicsContext 初始化阶段即物化并标脏，
     * 保证任何 world（含离屏 RenderOnce）第一帧拿到的就是有效 sky 数据。
     */
    GRAPH_MODULE_CLASS(EnvironmentManager)
    {
    public:

        /// ShadowInfo 按交换链图像分槽。slot_count == image_count（本引擎至少 3），
        /// 多帧同时在飞；单份 host-visible UBO 会被下一帧 CPU 覆写，主帧采样读到
        /// 别的帧的级联矩阵（阴影逐帧左右/远近跳）。8 覆盖常见 image_count。
        static constexpr uint32_t kShadowUboRing = 8;

        struct Profile
        {
            EnvProfileID    id = kEnvProfileInvalid;
            AnsiString      name;
            EnvironmentInfo cpu;                                               ///< CPU 侧唯一权威数据
            StructView<SkyInfo> *sky_ubo = nullptr;              ///< sky 段 GPU 物化（懒创建，default 例外）
            StructView<ShadowInfo> *shadow_ring[kShadowUboRing] = {}; ///< 每帧一份，按下标 = acquired image
        };

    private:

        EnvProfileID next_id = kEnvProfileDefault + 1;
        std::vector<Profile *> profiles;

        Profile *FindProfile(EnvProfileID id) const;
        Profile *FindProfile(const AnsiString &name) const;

        bool MaterializeSkyUBO(Profile *profile);
        bool MaterializeShadowUBO(Profile *profile);
        void ReleaseShadowRing(Profile *profile);

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

        /// 数据改完调用。sky 立即写入单份 UBO；shadow 只留在 CPU，
        /// 等交换链 acquire 之后由 CommitMaterialized 写入本帧槽。
        void MarkDirty(EnvProfileID id);

        /// sky 段 GPU buffer（绑定层用；懒物化，default 保证已就绪）
        const IGPUBuffer *GetSkyUBO(EnvProfileID id);

        /// shadow 段 GPU buffer（绑定层用）。frame_index 必须是本帧 acquired image，
        /// 与 CommitMaterialized 写入的槽一致；离屏 pass 没有在途交换链槽，传 0。
        const IGPUBuffer *GetShadowUBO(EnvProfileID id, uint32_t frame_index = 0);

        /// ViewUBOCommitSystem 专用：pass 开始固定写入。
        /// commit_shadow 仅在当前 RT 是交换链（acquire 已完成、该图像槽空闲）时为 true。
        void CommitMaterialized(uint32_t shadow_frame_index, bool commit_shadow);
    };//class EnvironmentManager
}//namespace hgl::graph
