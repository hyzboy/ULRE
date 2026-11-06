// ModelAsset.h — 异步场景句柄（阶段四 Step 4.1）
//
// ── 这个类解决什么 ────────────────────────────────────────────────────────
//
// 场景装载此前只有一条同步路径：调用方直接调 `LoadModel*`，
// 读盘、解包、建 GPU buffer、解析节点树全在主线程一口气做完。8 万面片的
// ABeautifulGame 一次性卡住主线程几百毫秒，而且调用方拿不到任何中间状态 ——
// 「还在装」这件事在类型系统里无法表达，只能靠调用方自己记一个 bool。
//
// `ModelAsset` 把这件事变成**句柄 + 状态机**，与 `TextureAsset` /
// `GeometryAsset` 同一个思路：
//
// ```
// Request(uri)      主线程立刻返回 Unloaded 句柄（0 IO、0 显存）
//    ↓  ModelStreamingSystem 每帧取 Unloaded 句柄派发
// Loading           模型包字节读取 + MDL2 解析在途
//    ↓  SetPlan()：拿到依赖计划单，子任务数确定
// Assembling        子项（几何 / 纹理 / 材质）在各自流送系统里推进
//    ↓  SettleChild() 逐项递减，全部落定 ⇒ CommitModel()
// Ready             可实例化进 ECS
// ```
//
// 也可以从任何状态掉到 `Failed`（不自动重试，避免坏文件每帧刷屏）。
//
// ── 为什么状态是「场景自己」而不是「每个子项」 ─────────────────────────────
//
// 子项的进度不由本类跟踪：几何在 `GeometryAssetRegistry` 的句柄表里、
// 纹理在 `TextureManager` 的句柄表里，那两张表才是子项状态的唯一真值
// （`TextureStreamingSystem` / `GeometryStreamingSystem` 的注释里已写明）。
// 本类只记「登记了几个子任务、落定了几个」这种**计数**，具体哪个子项没到
// 由流送系统去问句柄表 —— 这样不会出现「场景以为到了、句柄还没到」的双份真值。
//
// ── 与 `LevelAsset` 的区别 ────────────────────────────────────────────
//
// 见 `LevelAssetRegistry.h` 顶部对照表：那一种是手写规格书（TOML/FB），
// 这一种是 MDL2 二进制模型包。

#pragma once

#include <hgl/CoreType.h>
#include <hgl/graph/asset/ModelLoadPlan.h>
#include <hgl/graph/mesh/LoadStaticMesh.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>

#include <vector>

namespace hgl::graph
{
    class ModelAsset;
    class ModelAssetRegistry;
    class VulkanDevice;
    class GeometryManager;

    /// 场景句柄的生命周期状态。只由 `ModelStreamingSystem`（与测试）推动。
    enum class ModelAssetState : uint8
    {
        Unloaded = 0,   ///<只有 URI，尚未派发（流送系统每帧取它开工）
        Loading,        ///<模型包字节读取 / MDL2 解析在途
        Assembling,     ///<场景已解析，子项（几何/纹理/材质）在途
        Ready,          ///<全部就绪，可实例化进 ECS
        Failed,         ///<读取 / 解析 / 子项失败（不自动重试）
    };

    const char *GetModelAssetStateName(ModelAssetState state);

    /**
     * @brief 场景解析产物：可以立刻实例化进 ECS 的三张表。
     *
     * 为什么这里允许 `std::vector`：`LoadStaticMesh.h` 的整组入口
     * （`LoadModelFromMemory` / `...AsPrimitiveAssets`）的 out 参数就是
     * `std::vector`，那是网格层的既有 ABI。要接上它只有两条路 —— 改那一层的
     * 签名，或者在这里做一次容器类型让步。本类选后者，并把 `std::vector`
     * **限制在本结构体内**，其余新代码仍只用 HGL 容器。
     */
    struct ModelAssetContent
    {
        std::vector<PrimitiveAsset> assets;
        std::vector<StaticMeshNode> nodes;
        std::vector<int32>          root_nodes;

        bool IsEmpty()const { return assets.empty(); }

        void Clear()
        {
            assets.clear();
            nodes.clear();
            root_nodes.clear();
        }

        int GetAssetCount()const { return static_cast<int>(assets.size()); }
        int GetNodeCount ()const { return static_cast<int>(nodes.size()); }
        int GetRootCount ()const { return static_cast<int>(root_nodes.size()); }
    };

    /**
     * @brief 场景资产的异步句柄。
     *
     * 归属：由 `ModelAssetRegistry` 的表拥有并销毁（与 `GeometryAsset` 同款），
     * 材质 / 图元 / ECS 侧只持有裸观察指针，因此不存在「世界先死、句柄还在」。
     *
     * 线程约定：句柄只在主线程读写。后台 IO 线程只负责把字节读进来，
     * 状态推进一律在主线程（`ModelStreamingSystem::Update`）发生。
     */
    class ModelAsset
    {
    public:

        /// 计划单按派发种类汇总的结果（诊断与「这个场景有多大」的快速答案）
        struct Stats
        {
            int total = 0;              ///<任务总数
            int geometry = 0;           ///<外部几何（不含内嵌）
            int material = 0;           ///<材质容器
            int material_recipe = 0;    ///<配方
            int texture = 0;            ///<纹理
            int material_data = 0;      ///<材质数据块
            int other = 0;              ///<其余种类（场景/图元/环境）
            int embedded_geometry = 0;  ///<包内嵌几何份数（不产生 IO）
        };

        /// 就绪通知：在 `CommitModel()` 成功的同一调用内触发（可为空）
        using ReadyCallback = void (*)(ModelAsset *asset, void *user_data);

        /// 失败通知：在掉入 `Failed` 的同一调用内触发（可为空）
        using FailedCallback = void (*)(ModelAsset *asset, const char *reason, void *user_data);

    private:

        ModelAssetRegistry *registry = nullptr;     ///<所属表（不拥有）
        AnsiString          uri;                    ///<请求原文，同时是去重身份
        OSString            pack_path;              ///<已解析的物理/包裹路径（诊断用）

        ModelAssetState     state = ModelAssetState::Unloaded;
        ModelLoadPlan       plan;                   ///<依赖计划单（`SetPlan` 之后有效）
        Stats               stats;

        int                 child_total = 0;        ///<已登记的子任务数
        int                 child_ready = 0;        ///<已落定为就绪的子任务数
        int                 child_failed = 0;       ///<已落定为失败的子任务数

        uint64              async_request_id = 0;   ///<当前在途读盘请求号（诊断用）
        int                 async_attempt = 0;      ///<已发起过的次数（重试计数）

        AnsiString          fail_reason;

        ModelAssetContent   content;                ///<解析产物（`CommitModel` 之后有效）

        ReadyCallback       on_ready = nullptr;
        FailedCallback      on_failed = nullptr;
        void *              notify_user = nullptr;

        friend class ModelAssetRegistry;

    private:

        ModelAsset(ModelAssetRegistry *owner, const AnsiString &request_uri);

        /// 清空计划与计数（回到 Unloaded 时的复位动作）
        void ResetProgress();

        /// 按计划单重算 `stats` 与 `child_total`
        void RebuildStats();

    public:

        ModelAsset() = delete;
        ~ModelAsset() = default;

        ModelAsset(const ModelAsset &) = delete;
        ModelAsset &operator=(const ModelAsset &) = delete;

    public:     // 观察

        ModelAssetRegistry * GetRegistry  ()const { return registry; }
        const AnsiString &   GetURI       ()const { return uri; }
        const OSString &     GetPackPath  ()const { return pack_path; }

        ModelAssetState GetState()const { return state; }
        const char *    GetStateName()const { return GetModelAssetStateName(state); }

        /// `Loading` 或 `Assembling`（两者都表示「还在装」）
        bool IsLoading()const { return state == ModelAssetState::Loading
                                    || state == ModelAssetState::Assembling; }
        bool IsReady  ()const { return state == ModelAssetState::Ready; }
        bool HasFailed()const { return state == ModelAssetState::Failed; }

        /// 是否还需要流送系统干活（`Unloaded`：未派发；`Loading`/`Assembling`：在途）
        bool NeedsPump()const { return state == ModelAssetState::Unloaded || IsLoading(); }

        const ModelLoadPlan &GetPlan ()const { return plan; }
        const Stats &        GetStats()const { return stats; }

        const AnsiString &GetFailReason()const { return fail_reason; }

        /// 解析产物（`Assembling` 期间由流送系统写入，`Ready` 之后可读）
        const ModelAssetContent &GetContent()const { return content; }

        ModelAssetContent &GetMutableContent() { return content; }

        uint64 GetAsyncRequestID()const { return async_request_id; }
        int    GetAsyncAttempt  ()const { return async_attempt; }

        int GetChildCount       ()const { return child_total; }
        int GetReadyChildCount  ()const { return child_ready; }
        int GetFailedChildCount ()const { return child_failed; }
        /// 尚未落定的子任务数（`child_total - ready - failed`）
        int GetPendingChildCount()const { return child_total - child_ready - child_failed; }

        void SetNotifyCallback(ReadyCallback ready, FailedCallback failed, void *user_data)
        {
            on_ready    = ready;
            on_failed   = failed;
            notify_user = user_data;
        }

    public:     // 状态机（只由 ModelStreamingSystem / 测试写）

        /**
         * @brief 派发：`Unloaded` ⇒ `Loading`。
         * @param request_id 后台读盘请求号（仅诊断用，0 表示未跟踪）
         * @return 状态确实发生迁移返回 true（已在外载则返回 false，防重复派发）
         */
        bool BeginStreaming(uint64 request_id = 0);

        /**
         * @brief 模型包已解析：登记计划单与子任务数，`Loading` ⇒ `Assembling`。
         *
         * 子任务数为 0（空场景 / 只有内嵌几何且无需外部依赖）时会直接提交，
         * 因为此时没有可等待的子项。
         *
         * @param new_plan 依赖计划单（本类持有副本）
         * @param resolved_path 已解析的包路径（可为空，仅诊断用）
         * @return 是否进入 `Ready`（true = 已在本次调用内提交完成）
         */
        bool SetPlan(const ModelLoadPlan &new_plan, const OSString &resolved_path = OSString());

        /**
         * @brief 一个子任务落定。
         * @param ok 该子项是否就绪（false 记为失败，但不立刻让整场景失败 ——
         *           单个材质缺失应当降级显示，而不是整个场景打不开）
         * @return 全部子任务已落定返回 true
         */
        bool SettleChild(bool ok);

        /**
         * @brief 由流送系统按计划单状态汇总后批量落定（等价于连续调用 `SettleChild`）。
         *
         * 流送系统不逐项记账，而是逐帧去几何/纹理句柄表里查真值，然后把
         * 「本帧新落定了几项、其中几项失败」整体报回来 —— 这样计数只在一处
         * 累加，不会因为某个子项被跳过一次而永久少一项。
         *
         * @return 全部子任务已落定返回 true
         */
        bool SettleAllChildren(int ready_delta, int failed_delta);

        /**
         * @brief 用「实际图元数」替换计划单估算的子项数。
         *
         * 计划单只知道「这个场景会引用多少东西」，不知道解析后究竟有几个图元
         * 需要等几何。构建产物出来的那一帧调用本函数，把口径换成图元，
         * 只允许在 `Assembling` 且尚无子项落定时调用（否则计数会错位）。
         */
        void SetChildTotal(int total);

        /**
         * @brief 汇聚提交：子任务未全部落定返回 false；否则 ⇒ `Ready` 并触发通知。
         *
         * 允许在 `Assembling` 且 `GetPendingChildCount()==0` 时调用；
         * 也允许在没有子任务的 `Loading` 状态下直接调用。
         */
        bool CommitModel();

        /// 掉入 `Failed`（幂等；触发失败通知）
        void Fail(const char *reason);

        /// 回到 `Unloaded` 以便重试（保留 URI，清空计划与计数）
        void Reset();
    };//class ModelAsset

    /**
     * @brief 场景句柄表 —— 请求去重与所有权的唯一真值。
     *
     * 与 `GeometryAssetRegistry` / `TextureManager` 的分工完全对称：
     * 表拥有句柄，句柄不拥有 GPU 资源（GPU 资源在 `GeometryManager` 等原处）。
     *
     * 线程约定：只在主线程使用。
     */
    class ModelAssetRegistry
    {
    private:

        VulkanDevice *   device           = nullptr;    ///<不拥有，可为空（只算计划不需要设备）
        GeometryManager *geometry_manager = nullptr;    ///<不拥有，可为空

        ValueArray<ModelAsset *> assets;                ///<句柄表（拥有）

    private:

        ModelAsset *FindByURI(const AnsiString &request_uri)const;

    public:

        ModelAssetRegistry() = default;
        ~ModelAssetRegistry();

        ModelAssetRegistry(const ModelAssetRegistry &) = delete;
        ModelAssetRegistry &operator=(const ModelAssetRegistry &) = delete;

    public:

        /// 绑定目标设备 / 几何管理器（设备起来之后才拿得到）
        void SetTargets(VulkanDevice *dev, GeometryManager *geo_mgr)
        {
            device           = dev;
            geometry_manager = geo_mgr;
        }

        VulkanDevice *   GetDevice         ()const { return device; }
        GeometryManager *GetGeometryManager()const { return geometry_manager; }

    public:

        /**
         * @brief 取（无则建）场景句柄 —— **主线程零阻塞**。
         *
         * 同一个 URI 只会得到一个句柄（按请求原文去重），因此同一场景被多个
         * 世界请求时只有一次读盘、一份显存。新句柄停在 `Unloaded`，
         * 由 `ModelStreamingSystem` 每帧扫描派发。
         *
         * @return 空 URI 返回 nullptr
         */
        ModelAsset *Request(const AnsiString &uri);

        ModelAsset *Find(const AnsiString &uri)const { return FindByURI(uri); }

        /// 句柄表只读视图（流送系统逐帧扫描用）
        const ValueArray<ModelAsset *> &GetAssets()const { return assets; }

        int GetCount       ()const { return assets.GetCount(); }
        /// 还没就绪、也还没失败（= 正在装或等着装）
        int GetPendingCount()const;
        int GetReadyCount  ()const;
        int GetFailedCount ()const;
        /// 已登记的子任务总数（所有句柄合计，诊断用）
        int GetChildTaskCount()const;

    public:

        /// 销毁全部句柄（设备销毁前必须调用）
        void Clear();
    };//class ModelAssetRegistry

    /**
     * @brief 登记进程内默认场景句柄表（由 `GraphicsContext` 在设备初始化时登记）。
     *
     * 为什么需要它：计划书里的调用签名是 `RequestModelAsset(uri)` —— 调用方
     * 不该为了拿一个场景句柄而先摸到 `GraphicsContext`。设备级句柄表在全进程
     * 只有一份，因此这里按 `GetMountedAssetSource()` 同款方式暴露一个弱引用。
     */
    void SetDefaultModelAssetRegistry(ModelAssetRegistry *registry);
    ModelAssetRegistry *GetDefaultModelAssetRegistry();

    /**
     * @brief 统一异步入口：请求一个场景句柄，主线程立刻返回。
     *
     * 等价于 `GetDefaultModelAssetRegistry()->Request(uri)`。
     * 默认表未登记（设备尚未初始化）时返回 nullptr。
     */
    ModelAsset *RequestModelAsset(const AnsiString &uri);
}//namespace hgl::graph
