#pragma once

/**
 * 材质装载车道 —— 把「池 + 编译器 + 泵送节奏」这三件套收成一个可复用对象。
 *
 * ── 它解决的是什么 ───────────────────────────────────────────────────────
 *
 * `MaterialAsyncCompiler`（阶段三）只是「投递 / 提交」的桥，它不负责：
 *
 *   - 谁来创建并启动 `AssetCPUTaskPool`（一个池可以挂多个 sink，但总得有人起头）；
 *   - 谁在每帧调 `Pump() -- 且必须带着配方表 / 数据块表 / 纹理管理器；
 *   - 收尾顺序：池线程还在解析时销毁注册表 ⇒ UAF。
 *
 * 这三件事每个接入点都要重写一遍，写错一处就是崩溃。本类把它们固定下来：
 *
 * ```
 *   EnsureStarted()  ── 懒建池 + 挂编译器（幂等）
 *   BeginRecipeDirectory() / BeginDataBlockDirectory()  ── 投递整目录
 *   每帧 Pump(recipes, blocks, tm)  ── 主线程提交，微秒级
 *   IsIdle()  ── 门禁用：车道里还有没有在途任务
 *   Shutdown(...)  ── Drain → 再泵一次 → 摘编译器 → 回收池
 * ```
 *
 * ── 谁在用 ───────────────────────────────────────────────────────────────
 *
 * `GraphicsContext` 持有一个实例并对外暴露 `GetMaterialAsyncLoadLane()`，因此
 * 「材质解析在工作线程」这件事对上层是**有状态的资源**，不是散落的静态池：
 * 上下文销毁时车道必须一起收掉（早于配方表 `Clear()`）。
 *
 * `ModelStreamingSystem` 用它做**装载前门禁**：模型包字节到齐后，先把场景目录
 * 的配方整目录投给车道，`Build()` 等车道空闲再执行 —— 这样 `Build()` 里的
 * `FindOrLoadMaterialRecipe()` 不会再触发一次同步整目录扫描。
 *
 * ── 去重 ─────────────────────────────────────────────────────────────────
 *
 * 门禁是「整目录」粒度的，而调用方很常见的写法是**先同步把配方目录装一遍**，
 * 再请求场景（示例代码就是这么做的）。为了不把同一批文件重复解析，车道维护
 * 两层去重：
 *
 * - 落地去重：`SetDedupRegistries()` 注入的注册表里已有同名路径 ⇒ 跳过；
 * - 在途去重：本次扇出已投递、还没 `Pump()` 落地的路径 ⇒ 跳过。
 *
 * `BeginRecipeDirectory()` / `BeginDataBlockDirectory()` 的返回值是**真正投出去**
 * 的数量（跳过的不算），调用方可据此判断要不要等门禁。
 *
 * ── 线程约定 ─────────────────────────────────────────────────────────────
 *
 * 所有方法都**只能在主线程**调用（注册表无并发保护）。
 */

#include <hgl/graph/asset/MaterialAsyncCompile.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    class TextureManager;
}

namespace hgl::graph::mtl
{
    class MaterialRecipeRegistry;
    class MaterialDataBlockRegistry;
}

namespace hgl
{
    class LoadedFileManager;
}

namespace hgl::graph::mtl
{
    /**
     * 材质异步装载车道。
     *
     * @note 车道自建的任务池线程数默认为 2：配方 / 数据块都是小文件，
     *       线程多了只是在磁盘队列上互相打转。
     */
    class MaterialAsyncLoadLane
    {
        NO_COPY_NO_MOVE(MaterialAsyncLoadLane)

    private:

        hgl::asset::AssetCPUTaskPool *pool     = nullptr;
        MaterialAsyncCompiler        *compiler = nullptr;
        bool                          owns_pool = false;

        /// 文件投递用的读盘管理器（不持有）。未注入时 `Begin*File` 会失败。
        hgl::LoadedFileManager *io_manager = nullptr;

        /// 去重用的注册表（可空 ⇒ 不去重）。由持有者 `GraphicsContext` 注入。
        MaterialRecipeRegistry    *dedup_recipes = nullptr;
        MaterialDataBlockRegistry *dedup_blocks  = nullptr;

        /// 已投递、尚未落地的文件：同一文件在途时再投就是白跑一遍解析
        hgl::ManagedArray<OSString> in_flight;

        int submitted_recipes = 0;      ///< 累计投递的配方数
        int submitted_blocks  = 0;      ///< 累计投递的数据块数
        int pumped_commits    = 0;      ///< 累计提交成功的句柄数
        int skipped_existing  = 0;      ///< 因「表里已有 / 已在途」而跳过的文件数

        bool IsInFlight(const OSString &path) const;
        void MarkInFlight(const OSString &path);
        void ClearInFlightWhenIdle();

    public:

        MaterialAsyncLoadLane() = default;
        ~MaterialAsyncLoadLane();

        /**
         * @brief 注入去重注册表：表里已经有的、或已在途的文件不再投递。
         *
         * 为什么必须有：调用方常会「先同步把配方目录装一遍，再请求场景」。没有
         * 去重的话，装载前门禁会把同一批配方再解析一遍 —— 白烧两个线程、白建
         * 一批重名句柄（注册表按重名拒绝，日志里全是噪音）。
         *
         * @note 指针不持有所有权；注册表必须比车道活得久（`Shutdown` 要在 `Clear()` 之前）。
         */
        void SetDedupRegistries(MaterialRecipeRegistry *recipes,
                                MaterialDataBlockRegistry *blocks)
        {
            dedup_recipes = recipes;
            dedup_blocks  = blocks;
        }

        /**
         * @brief 注入读盘管理器：车道的文件投递经加载线程读字节，解析任务不再自己碰盘。
         *
         * @note 不持有。管理器必须活到所有已投递请求派发完毕
         *       （上下文销毁路径：先 `LoadedFileManager::Stop()`，再 `Shutdown()` 本车道）。
         */
        void AttachFileManager(hgl::LoadedFileManager *file_manager);

        /**
         * @brief 懒启动：建池 + 挂编译器（幂等）。
         *
         * @param worker_count 池线程数；`<=0` 时取默认 2
         * @return false 表示池启动失败
         */
        bool EnsureStarted(int worker_count = 2);

        /**
         * @brief 收尾：Drain 在途任务 → 用调用方给的表再泵一次 → 摘编译器 → 回收池。
         *
         * 注册表参数**可以**是 `nullptr`（那就丢弃成品），但在上下文销毁路径上
         * 应当把还活着的表传进来 —— 否则日志里会多出一堆「收到但没有注册表」。
         *
         * @note 必须在注册表 `Clear()` 之前调用，且此后车道不可再用。
         */
        void Shutdown(MaterialRecipeRegistry *recipes = nullptr,
                      MaterialDataBlockRegistry *blocks = nullptr,
                      hgl::graph::TextureManager *texture_manager = nullptr);

        bool IsRunning() const { return compiler != nullptr; }

        // ── 投递（主线程）────────────────────────────────────────────────

        /// 递归扫描目录，把其中所有 `.recipe.toml` 投给工作线程；返回投递数，`<0` 失败
        int BeginRecipeDirectory(const OSString &directory);

        /// 递归扫描目录，把其中所有数据块文件投给工作线程；返回投递数，`<0` 失败
        int BeginDataBlockDirectory(const OSString &directory);

        /// 单个配方文件；已落地 / 已在途也算成功（「已就位」），不去重时才会重投
        bool BeginRecipeFile(const OSString &path);

        /// 单个数据块文件；语义同 BeginRecipeFile
        bool BeginDataBlockFile(const OSString &path);

        // ── 提交（主线程，微秒级）────────────────────────────────────────

        /**
         * @brief 把已完成的结果提交进表。
         *
         * `recipes` / `blocks` 可各自为 `nullptr`（只处理另一类）。
         * @return 本次成功提交的句柄数
         */
        int Pump(MaterialRecipeRegistry *recipes,
                 MaterialDataBlockRegistry *blocks,
                 hgl::graph::TextureManager *texture_manager);

        // ── 状态（门禁用）────────────────────────────────────────────────

        /// 车道里没有在途任务（未启动也算空闲）
        bool IsIdle() const;

        int GetPendingCount() const;
        int GetSubmittedRecipeCount() const { return submitted_recipes; }
        int GetSubmittedDataBlockCount() const { return submitted_blocks; }
        int GetPumpedCommitCount() const { return pumped_commits; }
        int GetSkippedCount() const { return skipped_existing; }
        int GetFailedCount() const;
    };//class MaterialAsyncLoadLane
}//namespace hgl::graph::mtl
