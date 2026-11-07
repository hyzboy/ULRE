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
 *   Submit*File() ──► LoadedFileManager 加载线程读盘 ─┐
 *                                                  ├─► CPU 池工作线程 Execute()（纯解析）
 *   Submit*Source() ───────────────────────────────┘            │
 *                                        终态事件全部压进 inbox ──► 主线程 Pump()
 *                                                                  │
 *                                             RegisterParsedRecipe / CommitParsed
 * ```
 *
 * - `Submit*()` 与 `Pump()` 都**只能在主线程**调用（注册表本身没有并发保护）；
 *   读盘中转 `OnTaskEvent` 在 IO/Pump 线程回调，只干「拷字节、转投解析任务」的轻活。
 * - 任务对象所有权：提交后归执行线程，终态事件经信息池回到本类，`Pump()` 消费并
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

#include <hgl/thread/TaskNotification.h>

#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/asset/MaterialRecipeAsset.h>

#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/mtl/MaterialRecipeFile.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>

#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

namespace hgl
{
    class LoadedFileManager;
}

namespace hgl::graph
{
    class TextureManager;
}

namespace hgl::graph::mtl
{
    /**
     * 配方异步解析任务：**纯内存解析，不碰盘**（字节由 `LoadedFileManager` 或调用方备好）。
     *
     * 产物 `data` / `spec` 就是 `MaterialRecipeAsset` 的构造实参，主线程把它转交
     * `MaterialRecipeRegistry::RegisterParsedRecipe()` 即可，不需要重新解析。
     */
    class MaterialRecipeParseTask final : public hgl::asset::AssetCPUTask
    {
        NO_COPY_NO_MOVE(MaterialRecipeParseTask)

    private:

        OSString label;                     ///< 诊断用来源标识（文件路径或 `<memory>:...`）
        bool     parse_ok  = false;         ///< TOML 解析是否成功

        hgl::ValueArray<char>   memory;     ///< 待解析的文本（构造时拷贝）
        MaterialRecipeFileData  data;       ///< 解析产物（`parse_error` 在失败时有效）
        MaterialRecipeSpec      spec;       ///< 授权形态（提交时被移走）

    public:

        /// 字节在构造时拷贝进来（调用方缓冲可立刻释放）
        MaterialRecipeParseTask(const os_char *label, const char *content, int content_size);

        bool Execute() override;

        const OSString &GetLabel() const { return label; }
        bool IsParseOk() const { return parse_ok; }

        /// 提交时移走产物（`Pump()` 唯一用法）
        MaterialRecipeFileData &GetData() { return data; }
        MaterialRecipeSpec     &GetSpec() { return spec; }
    };//class MaterialRecipeParseTask

    /**
     * 数据块异步解析任务：按扩展名分派 `.material_data.toml` / `.mdb` / `.fb`。
     *
     * 解析失败**不是**任务失败 —— `data.parse_error` 里有行号与键值，主线程要拿它
     * 建 `Failed` 句柄让工具能显示。只有「拿不到字节」才算失败，而字节由
     * `LoadedFileManager`（文件投递）或调用方（内存投递）备好，任务本身不读盘。
     */
    class MaterialDataBlockParseTask final : public hgl::asset::AssetCPUTask
    {
        NO_COPY_NO_MOVE(MaterialDataBlockParseTask)

    private:

        OSString                label;
        bool                    parse_ok  = false;
        bool                    has_spec  = false;

        hgl::ValueArray<uint8>    memory;   ///< 待解析的字节（构造时拷贝）
        MaterialDataBlockFileData data;
        MaterialDataBlockSpec     spec;     ///< 解析失败时为空
        AnsiString                failure_reason;

        bool ExecuteBytes(const uint8 *bytes, uint64 size);

    public:

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
     * 文件类投递不再让解析任务自己读盘：统一经 `LoadedFileManager`（加载线程）
     * 读字节，到齐后由 `FileReadRelay` 中转成纯内存解析任务投给 CPU 池。
     * 两条腿的终态事件都汇进同一个 `inbox` 信息池，`Pump()` 一次批量交换处理整批
     * （一个线程一次跑完一堆任务时，只需一次锁交换）。
     */
    class MaterialAsyncCompiler
    {
        NO_COPY_NO_MOVE(MaterialAsyncCompiler)

    private:

        /// 一次文件读取的上下文：路径 + 任务类型。随 `task_user_data` 原样回投，
        /// 由中转器在终态时释放。
        struct FileReadContext
        {
            OSString path;
            bool     is_recipe = false;
        };

        /// IO 读盘终态中转器：可能在 IO 线程或 Pump 线程被回调，只干轻活
        /// （拷字节转投解析任务；转投未遂时压一个不带任务的终态事件进 inbox）。
        class FileReadRelay final : public hgl::thread::TaskReceiver
        {
            NO_COPY_NO_MOVE(FileReadRelay)

        private:

            MaterialAsyncCompiler *owner = nullptr;

        public:

            explicit FileReadRelay(MaterialAsyncCompiler *owner_in) : owner(owner_in) {}

            void OnTaskEvent(const hgl::thread::TaskEvent &event) override;
        };//class FileReadRelay

    private:

        hgl::asset::AssetCPUTaskPool *pool = nullptr;
        hgl::LoadedFileManager       *io_manager = nullptr;
        uint32                        channel_id = 0;

        /// 终态事件汇（多生产者、主线程单消费者）：带任务指针的事件拥有该任务。
        hgl::thread::ThreadMessagePool<hgl::thread::TaskEvent> inbox;
        hgl::thread::InboxTaskReceiver cpu_receiver{ &inbox };
        FileReadRelay                  io_relay;

        int pending        = 0;     ///< 已投递、尚未 `Pump` 结算的任务数（仅主线程改动）
        int recipe_done    = 0;
        int block_done     = 0;
        int failed         = 0;
        int cancelled      = 0;

        bool SubmitTask(hgl::asset::AssetCPUTask *task);
        bool SubmitFileToRead(const OSString &path, bool is_recipe);

        void HandleTaskEvent(const hgl::thread::TaskEvent &event,
                             MaterialRecipeRegistry *recipes,
                             MaterialDataBlockRegistry *blocks,
                             hgl::graph::TextureManager *texture_manager,
                             int &committed);

    public:

        MaterialAsyncCompiler();
        ~MaterialAsyncCompiler();

        /**
         * @brief 挂到共享 CPU 池上。
         *
         * @param task_pool   池（调用方持有；必须已 `Start()`）
         * @param channel_id  建议的 IO 通道号（供 `AsyncFileLoadRequest::target_id` 使用）
         * @return false 表示参数非法或池未启动
         */
        bool Attach(hgl::asset::AssetCPUTaskPool *task_pool,
                    uint32 channel_id = 0x4D415441u /* 'MATA' */);

        /**
         * @brief 注入文件读取用的 `LoadedFileManager`（不持有）。
         *
         * `Submit*File` / `Submit*DirectoryFiles` 依赖它；内存投递不受影响。
         * @note 管理器必须活到所有已投递请求派发完毕（`Stop(Drain)` 早于本对象销毁）。
         */
        void AttachFileManager(hgl::LoadedFileManager *file_manager) { io_manager = file_manager; }

        hgl::LoadedFileManager *GetFileManager() const { return io_manager; }

        /// 从池上摘下（须先 `Pump()` 干净；否则返回 false）
        /// @note 池的生命周期必须覆盖本对象：销毁池之前必须先 `Detach()`
        bool Detach();

        bool IsAttached() const { return pool != nullptr; }

        /// 供 `AsyncFileLoadRequest::target_id` 使用的通道号
        uint32 GetChannelID() const { return channel_id; }

        // ── 投递（主线程）────────────────────────────────────────────────

        /// 一个配方文件；经 `LoadedFileManager` 读盘，字节到齐后投工作线程解析
        bool SubmitRecipeFile(const OSString &path);

        /// 已读进内存的配方文本
        bool SubmitRecipeSource(const os_char *label, const char *content, int content_size);

        /// 扫目录（递归）投递所有 `.recipe.toml`；返回投递数，`<0` 表示目录不可用
        int SubmitRecipeDirectoryFiles(const OSString &directory);

        /// 一个数据块文件；经 `LoadedFileManager` 读盘，字节到齐后投工作线程解析
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
