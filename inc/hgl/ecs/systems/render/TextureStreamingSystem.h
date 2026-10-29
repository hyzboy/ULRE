#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/module/TextureLoadCommitter.h>
#include<hgl/type/String.h>

namespace hgl
{
    class LoadedFileManager;

    namespace graph
    {
        class TextureAsset;
        class TextureManager;
    }
}

namespace hgl::ecs
{
    /**
     * 纹理流式加载系统 —— 把"句柄"变成"可绑定的纹理"。
     *
     * 为什么需要它：作者只在配方里写 `recipe.AddTexture("Diffuse", asset)`，
     * 这时材质手里的 `graph::TextureAsset` 只有文件名（`MetaOnly`），显存里什么都没有。
     * 谁来把文件读进来、谁在上传完成后把句柄转成 `Resident`、谁决定图元何时可以渲染,
     * 这些问题**不能散落在每个示例里**（那就退化成手工两段式 `AsyncTextureUpload`）。
     * 本系统是这条链路的唯一引擎侧驱动者。
     *
     * 工作集 = `TextureManager` 的**句柄表**，不另存一份订阅名单：
     * 「有文件名 + 未就绪 + 未在途」就是待办集合，句柄表本身即唯一真值。
     * 因此本系统**完全不接触 ECS 组件/实体**，只是一台句柄状态机。
     *
     * 每帧两段，相位固定为 `ExecutionPhase::RenderPreBeginFrame`
     * （必须早于引擎紧随其后的 `TextureManager::UpdateUploadQueue`，
     *   否则本帧提交的上传要等到下一帧才可能完成）：
     *
     * ```
     * ① Update()
     *      LoadedFileManager::Pump()             ← 派发上一帧读盘完成的回调
     *        OnFileLoaded → TextureLoadCommitter  ← 解析字节，提交 GPU 上传
     *      扫描句柄表：待办句柄 ⇒ Submit() + MarkAsyncRequested(id)
     *        已由同步路径加载过同一文件 ⇒ 直接 CommitAsyncTexture(命中项)
     *      LoadedFileManager::Collect()           ← 回收已归零的文件引用
     * ② （相位之后，引擎自己做的）TextureManager::UpdateUploadQueue
     *       上传完成回调 → TextureAsset::CommitAsyncTexture(tex)
     * ```
     *
     * 就绪判定与"未就绪不渲染"不在这里：`PrimitiveState::CanRender` 每帧现算
     * `MaterialData::HasPendingRequiredTexture()`（无缓存），句柄转入 `Resident` 的
     * 同一帧就能被收集看到，所以无需任何失效通知。
     *
     * 失败语义：读取或上传失败 ⇒ 句柄停在 `LoadFailed`，本系统**不自动重试**
     * （避免坏文件每帧重读磁盘刷屏）；该图元持续不渲染，并留下 error 日志。
     * 需要重试时由调用方 `ClearAsyncRequest()` 把它放回待办集合。
     */
    class TextureStreamingSystem final : public System
    {
        class StreamHandler;

    private:

        StreamHandler *handler          = nullptr;  ///< 读盘回调接收者(活到最后一个回调)
        LoadedFileManager *load_manager = nullptr;
        graph::TextureManager *tex_manager = nullptr;
        graph::TextureLoadCommitter committer;

    private:

        bool EnsureManagers();                          ///< 惰性抓取上下文里的两个管理器
        void AcquireOneFrame();                         ///< 提交本帧该发起的读取
        graph::TextureAsset * FindAssetByRequest(uint64 request_id)const;

    public:

        TextureStreamingSystem();
        ~TextureStreamingSystem() override;

        void Update(float delta_time) override;
        void Shutdown() override;

        int GetPendingRequestCount()const;
    };//class TextureStreamingSystem
}//namespace hgl::ecs
