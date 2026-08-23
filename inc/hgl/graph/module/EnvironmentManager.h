#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/ubo/EnvironmentInfo.h>
#include<hgl/type/String.h>

#include<vector>

namespace hgl::graph
{
    class BufferManager;
    class IGPUBuffer;

    template<typename T> class StructuredBufferAccessor;

    /**
     * EnvironmentManager - 环境综合信息统一管理器
     *
     * 集中持有所有环境 Profile（数据 + GPU 物化），设备级唯一（GraphicsContext 模块）。
     * RT/WORLD 不拥有环境数据，只持有 EnvProfileID 引用；未设置即用内置 default。
     *
     * 分层约定：
     * - 数据层：EnvironmentInfo（纯数据，CPU 侧唯一权威在 Profile::cpu）
     * - 管理层：本类（Profile 注册 / GPU UBO 物化 / 脏标记）
     * - 选择层：IRenderTarget::GetEnvironmentProfile()（未设置 = kEnvProfileDefault）
     * - 绑定层：RenderDescriptorBindingSystem 按 RT 选择解析 GetSkyUBO() 写入 Scene Set
     *
     * GPU 上传统一走设备级 dirty 扫描（RenderBufferUploadSystem），
     * default Profile 在 GraphicsContext 初始化阶段即物化并标脏，
     * 保证任何 world（含离屏 RenderOnce）第一帧拿到的就是有效 sky 数据。
     */
    GRAPH_MODULE_CLASS(EnvironmentManager)
    {
    public:

        struct Profile
        {
            EnvProfileID    id = kEnvProfileInvalid;
            AnsiString      name;
            EnvironmentInfo cpu;                                               ///< CPU 侧唯一权威数据
            StructuredBufferAccessor<SkyInfo> *sky_ubo = nullptr;              ///< sky 段 GPU 物化（懒创建，default 例外）
        };

    private:

        EnvProfileID next_id = kEnvProfileDefault + 1;
        std::vector<Profile *> profiles;

        Profile *FindProfile(EnvProfileID id) const;
        Profile *FindProfile(const AnsiString &name) const;

        bool MaterializeSkyUBO(Profile *profile);

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

        /// 数据改完调用；由设备级 dirty 扫描统一上传
        void MarkDirty(EnvProfileID id);

        /// sky 段 GPU buffer（绑定层用；懒物化，default 保证已就绪）
        const IGPUBuffer *GetSkyUBO(EnvProfileID id);

        /// ViewUBOCommitSystem 专用：pass 开始时把所有已物化 profile 的 sky 段
        /// 无条件全量写入 GPU（不依赖脏标记）
        void CommitMaterialized();
    };//class EnvironmentManager
}//namespace hgl::graph
