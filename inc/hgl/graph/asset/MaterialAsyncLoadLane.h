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

        /// 本车道的 sink 编号。与其它 sink（IO 分发器 0x494F2F31、
        /// 场景取流等）不得冲突，故取一个独立的魔数。
        enum : hgl::uint32 { kLaneSinkID = 0x4D41544Cu /* 'MATL' */ };

        hgl::asset::AssetCPUTaskPool *pool     = nullptr;
        MaterialAsyncCompiler        *compiler = nullptr;
        bool                          owns_pool = false;

        int submitted_recipes = 0;      ///< 累计投递的配方数
        int submitted_blocks  = 0;      ///< 累计投递的数据块数
        int pumped_commits    = 0;      ///< 累计提交成功的句柄数

    public:

        MaterialAsyncLoadLane() = default;
        ~MaterialAsyncLoadLane();

        /**
         * @brief 懒启动：建池 + 挂编译器（幂等）。
         *
         * @param worker_count 池线程数；`<=0` 时取默认 2
         * @return false 表示池启动失败或 sink 编号被占用
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

        /// 单个配方文件
        bool BeginRecipeFile(const OSString &path);

        /// 单个数据块文件
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
        int GetFailedCount() const;
    };//class MaterialAsyncLoadLane
}//namespace hgl::graph::mtl
