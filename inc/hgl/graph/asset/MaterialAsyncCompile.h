#pragma once

/**
 * 材质子系统的异步解析落地层（三级流水线的 Tier 2 → Tier 3 桥）。
 *
 * ── 为什么需要这一层 ─────────────────────────────────────────────────────
 *
 * 配方（`.recipe.toml`）与数据块（`.material_data.toml` / `.mdb` / `.fb`）的
 * **解析是纯 CPU 的**：TOML 文本 → `MaterialRecipeFileData` / `MaterialDataBlockFileData`
 * 全程不碰 GPU。真正需要主线程的只有两件事：
 *
 *   - 建句柄（`ManagedArray<...>::Create`，无并发保护）；
 *   - 纹理句柄创建（`TextureManager::CreateTextureAsset`）与 SSBO 上传。
 *
 * 同步入口（`MaterialRecipeRegistry::LoadFile/Directory`、
 * `MaterialDataBlockRegistry::LoadFile/Directory`）把这两件事和文本解析串在
 * 一起，于是一个几百 KB 的配方目录就会在主线程上卡出毫秒级尖峰。本层把
 * 「读字节 + 解析」搬到 `hgl::asset::AssetCPUTaskPool` 的工作线程，主线程只
 * 剩下 `Pump()` 里的「建句柄 → 多轮继承推进」——微秒级，可逐帧调用。
 *
 * ── 线程与所有权 ─────────────────────────────────────────────────────────
 *
 * ```
 *   主线程 Submit*() ──► 工作线程 Execute()（读文件 + 解析）──► 主线程 Pump()
 *                                                                  │
 *                                             RegisterParsedRecipe / CommitParsed
 * ```
 *
 * - `Submit*()` 与 `Pump()` 都**只能在主线程**调用（注册表本身没有并发保护）。
 * - 任务对象所有权：提交后归池，完成后经 sink 回到本类的邮箱，`Pump()` 消费并
 *   `delete`。调用方永远不持有任务指针。
 * - `Pump()` 只做提交，不做 I/O、不做解析；单条提交成本与配方里纹理槽数量线性相关。
 *
 * ── 与同步入口的关系 ─────────────────────────────────────────────────────
 *
 * 两条路最终落在同一组提交函数上：
 *
 * - 配方：`RegisterRecipeSource`（同步）与 `RegisterParsedRecipe`（异步落地）共享
 *   「重名检查 → 建句柄 → `ResolvePending`」全部逻辑，差别只在谁做 TOML 解析。
 * - 数据块：`RegisterFromToml/Binary/Flat` 与 `CommitParsed` 共享 `Attach`，
 *   解析失败同样建 `Failed` 句柄留表（诊断信息是工具要显示的内容）。
 *
 * 因此异步加载的**语义与同步完全一致**：父配方后到照样能补上（`ResolvePending`
 * 的多轮推进没变），坏输入照样留诊断，重名照样拒。
 *
 * ── 已把文件读进内存的调用方（走 IO 分发器）─────────────────────────────
 *
 * 若调用方已经用 `AssetIODispatcher` 批量读好了字节，走 `Submit*Source()`
 * 直接投解析任务即可，不必让工作线程再读一次盘：把
 * `hgl::asset::AsyncFileLoadRequest::target_id` 设成 `GetChannelID()`，在
 * 自己的 `OnFileLoaded()` 里按 `target_id` 分流到本类的 `Submit*Source()`。
 */

#include <hgl/asset/AssetCPUTask.h>
#include <hgl/asset/AssetCPUTaskPool.h>

#include <hgl/concurrency/SwapMailbox.h>

#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/asset/MaterialRecipeAsset.h>

#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>

#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph
{
    class TextureManager;
}

namespace hgl::graph::mtl
{
    /**
     * 配方异步解析任务：**读文件 + TOML 解析都在工作线程**。
     *
     * 产物 `data` / `spec` 就是 `MaterialRecipeAsset` 的构造实参，主线程把它转交
     * `MaterialRecipeRegistry::RegisterParsedRecipe()` 即可，不需要重新解析。
     */
    class MaterialRecipeParseTask final : public hgl::asset::AssetCPUTask
    {
        NO_COPY_NO_MOVE(MaterialRecipeParseTask)

    private:

        OSString label;                     ///< 诊断用来源标识（文件路径或 `<memory>:...`）
        bool     file_mode = true;          ///< `true` = 工作线程自己读盘；`false` = 用构造时拷贝的字节
        bool     read_ok   = false;         ///< 字节读取是否成功（内存模式恒为 `true`）
        bool     parse_ok  = false;         ///< TOML 解析是否成功

        hgl::ValueArray<char>   memory;     ///< 内存模式的文本（构造时拷贝）
        MaterialRecipeFileData  data;       ///< 解析产物（`parse_error` 在失败时有效）
        MaterialRecipeSpec      spec;       ///< 授权形态（提交时被移走）

    public:

        /// 文件模式：工作线程里自己读盘
        explicit MaterialRecipeParseTask(const OSString &path);

        /// 内存模式：字节在构造时拷贝进来（调用方缓冲可立刻释放）
        MaterialRecipeParseTask(const os_char *label, const char *content, int content_size);

        bool Execute() override;

        const OSString &GetLabel() const { return label; }
        bool IsReadOk() const { return read_ok; }
        bool IsParseOk() const { return parse_ok; }

        /// 提交时移走产物（`Pump()` 唯一用法）
        MaterialRecipeFileData &GetData() { return data; }
        MaterialRecipeSpec     &GetSpec() { return spec; }
    };//class MaterialRecipeParseTask

    /**
     * 数据块异步解析任务：按扩展名分派 `.material_data.toml` / `.mdb` / `.fb`。
     *
     * 解析失败**不是**任务失败 —— `data.parse_error` 里有行号与键值，主线程要拿它
     * 建 `Failed` 句柄让工具能显示。只有「读不到字节」才算任务失败。
     */
    class MaterialDataBlockParseTask final : public hgl::asset::AssetCPUTask
    {
        NO_COPY_NO_MOVE(MaterialDataBlockParseTask)

    private:

        OSString                label;
        bool                    file_mode = true;
        bool                    parse_ok  = false;
        bool                    has_spec  = false;

        hgl::ValueArray<uint8>    memory;   ///< 内存模式的字节（构造时拷贝）
        MaterialDataBlockFileData data;
        MaterialDataBlockSpec     spec;     ///< 解析失败时为空
        AnsiString                failure_reason;

        bool ExecuteBytes(const uint8 *bytes, uint64 size);

    public:

        explicit MaterialDataBlockParseTask(const OSString &path);
        MaterialDataBlockParseTask(const os_char *label, const uint8 *bytes, uint64 size);

        bool Execute() override;

        const OSString &GetLabel() const { return label; }
        bool IsParseOk() const { return parse_ok; }
        bool HasSpec() const { return has_spec; }

        /// 失败诊断（`\0` 结尾，提交时直接喂给 `CommitParsed`）
        const char *GetFailureReason() const
        {
            return failure_reason.IsEmpty() ? nullptr : failure_reason.c_str();
        }

        MaterialDataBlockFileData &GetData() { return data; }
        MaterialDataBlockSpec     &GetSpec() { return spec; }
    };//class MaterialDataBlockParseTask

    /**
     * 材质异步编译器：投递解析任务 + 主线程提交。
     *
     * 一个 `AssetCPUTaskPool` 可以同时挂多个编译器（IO 分发器、纹理、材质各一个），
     * 各自持有一个 sink id 与自己的邮箱；池不做业务区分。
     */
    class MaterialAsyncCompiler
    {
        NO_COPY_NO_MOVE(MaterialAsyncCompiler)

    private:

        /// 成品邮箱：池的工作线程 `Post`，主线程 `Take`
        class MaterialAsyncBox final : public hgl::asset::AssetCPUTaskSink
        {
            NO_COPY_NO_MOVE(MaterialAsyncBox)

        private:

            hgl::SwapMailbox<hgl::asset::AssetCPUTask *> done;

        public:

            MaterialAsyncBox() = default;

            void OnAssetCPUTaskReady(hgl::asset::AssetCPUTask *task) override;
            int  Take(hgl::ValueArray<hgl::asset::AssetCPUTask *> &out);
            void Close();
        };//class MaterialAsyncBox

    private:

        hgl::asset::AssetCPUTaskPool *pool    = nullptr;
        uint32                        sink_id = 0;
        uint32                        channel_id = 0;

        MaterialAsyncBox box;

        int pending        = 0;     ///< 已投递、尚未 `Pump` 掉的任务数
        int recipe_done    = 0;
        int block_done     = 0;
        int failed         = 0;
        int cancelled      = 0;

        bool SubmitTask(hgl::asset::AssetCPUTask *task);

    public:

        MaterialAsyncCompiler() = default;
        ~MaterialAsyncCompiler();

        /**
         * @brief 挂到共享 CPU 池上。
         *
         * @param task_pool   池（调用方持有；必须已 `Start()`）
         * @param sink_id     本编译器在池里的 sink 编号（与其它 sink 不得重复）
         * @param channel_id  建议的 IO 通道号（供 `AsyncFileLoadRequest::target_id` 使用）
         * @return false 表示参数非法或 sink 已被占用
         */
        bool Attach(hgl::asset::AssetCPUTaskPool *task_pool,
                    uint32 sink_id,
                    uint32 channel_id = 0x4D415441u /* 'MATA' */);

        /// 从池上摘下（须先 `Pump()` 干净；否则返回 false）
        /// @note 池的生命周期必须覆盖本对象：销毁池之前必须先 `Detach()`
        bool Detach();

        bool IsAttached() const { return pool != nullptr; }

        /// 供 `AsyncFileLoadRequest::target_id` 使用的通道号
        uint32 GetChannelID() const { return channel_id; }

        // ── 投递（主线程）────────────────────────────────────────────────

        /// 一个配方文件；工作线程读盘 + 解析
        bool SubmitRecipeFile(const OSString &path);

        /// 已读进内存的配方文本
        bool SubmitRecipeSource(const os_char *label, const char *content, int content_size);

        /// 扫目录（递归）投递所有 `.recipe.toml`；返回投递数，`<0` 表示目录不可用
        int SubmitRecipeDirectoryFiles(const OSString &directory);

        /// 一个数据块文件；工作线程读盘 + 解析
        bool SubmitDataBlockFile(const OSString &path);

        /// 已读进内存的数据块字节
        bool SubmitDataBlockSource(const os_char *label, const uint8 *bytes, uint64 size);

        /// 扫目录（递归）投递所有数据块文件；返回投递数，`<0` 表示目录不可用
        int SubmitDataBlockDirectoryFiles(const OSString &directory);

        // ── 提交（主线程，微秒级）────────────────────────────────────────

        /**
         * @brief 把已完成的结果提交进注册表。
         *
         * `recipes` / `blocks` 可各自为 `nullptr`（只处理另一类）。`texture_manager`
         * 与同步路径同义：为 `nullptr` 时带纹理槽的配方会在校验阶段失败。
         *
         * @return 本次成功提交的句柄数
         */
        int Pump(MaterialRecipeRegistry *recipes,
                 MaterialDataBlockRegistry *blocks,
                 hgl::graph::TextureManager *texture_manager);

        // ── 状态 ─────────────────────────────────────────────────────────

        bool IsIdle() const { return pending == 0; }
        int  GetPendingCount() const { return pending; }
        int  GetRecipeCommittedCount() const { return recipe_done; }
        int  GetDataBlockCommittedCount() const { return block_done; }
        int  GetFailedCount() const { return failed; }
        int  GetCancelledCount() const { return cancelled; }

        void ClearStatistics()
        {
            recipe_done = 0;
            block_done  = 0;
            failed      = 0;
            cancelled   = 0;
        }
    };//class MaterialAsyncCompiler
}//namespace hgl::graph::mtl
