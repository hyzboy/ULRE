#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/graph/asset/ModelAsset.h>
#include<hgl/graph/mesh/ModelStreaming.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/type/String.h>
#include<hgl/type/ValueArray.h>

namespace hgl
{
    class LoadedFileManager;

    namespace graph
    {
        class GeometryManager;
    }
}

namespace hgl::ecs
{
    /**
     * 场景流送系统 —— 把「一个 URI」变成「可以立刻实例化进 ECS 的图元 + 节点树」。
     *
     * ## 为什么需要它
     *
     * 模型包是**一个**文件，但里面装着几何、材质、配方、纹理的引用。把它拆成
     * 「谁先谁后」的活由阶段三的 `BuildModelLoadPlan` 完成，把「谁在等谁」的记账
     * 由 `ModelAsset` 句柄完成；本系统只负责**推动**：读盘、解析、逐帧结算，
     * 全部发生在主线程，任何一帧都不阻塞。
     *
     * ## 每帧固定相位（`ExecutionPhase::RenderPreBeginFrame`）
     *
     * ```text
     * ① LoadedFileManager::Pump()   派发上一帧完成的读盘（字节落到句柄自己的缓冲）
     * ② PumpMaterialLane()          把材质车道里解析好的配方提交进配方表（微秒级）
     * ③ AcquireOneFrame()           本帧最多发起 kMaxSubmitPerFrame 个新句柄的取流
     * ④ Advance()                   推进每个句柄：建计划 ⇒ 扇出配方 ⇒ 解析产物 ⇒ 结算 ⇒ 提交
     * ⑤ LoadedFileManager::Collect() 回收已归零的文件引用
     * ```
     *
     * ## 取流通道（全部经由 `LoadedFileManager`）
     *
     * | 情形 | 查表键 | 字节来源 |
     * |---|---|---|
     * | URI 能折成物理路径（裸路径 / 目录型包裹里的真实文件） | 物理路径 | 读盘线程直接读文件 |
     * | 折不成物理路径（`id:` 身份寻址、归档型包裹、`builtin`/`rt`） | URI 原文（不与真实路径撞车） | `asset::AssetURIByteSource` 经 `OpenAssetStream` 分派 |
     *
     * 两条通道过去各自为政：第二条曾由主线程同步 `OpenAssetInputStream()` +
     * 整包 `Read()` 完成（大模型每帧几十毫秒的卡顿正是它造成的），后来改自养
     * CPU 池取流。现在统一进同一个读盘线程池：字节源只是把「从哪取字节」
     * 下推到 IO worker，取消、去重、预算、回调全部复用同一条链路。
     *
     * ## 「子项」的口径
     *
     * 场景句柄等的是**图元**，不是计划单里的每一份文件：
     *   · 材质 / 配方 / 材质数据在解析期（`Build`）里同步完成，无需等待；
     *   · 纹理由 `TextureStreamingSystem` 自行按需流送，且渲染侧每帧现算
     *     `MaterialData::HasPendingRequiredTexture()` —— 缺一张纹理应当降级显示，
     *     不该让整个场景打不开，因此**不阻塞场景就绪**；
     *   · 几何才是真正的门禁：图元几何没好就没法画。
     *
     * 因此构建产物出来之后，句柄的子项口径会被换成「图元数」（`SetChildTotal`），
     * 每帧统计 `PrimitiveAsset::IsGeometryReady()` 的真实结果再批量结算。
     * 计划单里的纹理/材质项只用于 `Stats` 诊断。
     *
     * 两条收敛路径都必须显式 `CommitModel()`：`SettleAllChildren` 只记账，不推状态机 ——
     * 落了账却不提交，句柄会永远停在 `Assembling`。
     * 几何 `LoadFailed` 是终态（`GeometryStreamingSystem` 不重试），必须按**失败**落定，
     * 否则该图元永远等不到，场景同样卡在 `Assembling`。
     *
     * ## 外部设定
     *
     * `GraphicsContext` 不持有几何顶点格式与材质配方 —— 那是调用方（示例/工具）
     * 自己的约定。因此调用方必须在请求场景之前 `SetBuildSettings(...)`；
     * 未设定时句柄直接失败并给出原因，而不是静默画出一个没有材质的场景。
     */
    class ModelStreamingSystem final : public System
    {
        class StreamHandler;

    public:

        /// 解析模型包所需的外部设定。
        struct BuildSettings
        {
            graph::GeometryVertexFormat geometry_vertex_format;

            /// 缺少配方时的兜底材质（可为空 ⇒ 走引擎默认）
            const graph::mtl::MaterialRecipe *default_recipe = nullptr;

            /// 场景内材质名 → 配方的查表（可为空 ⇒ 全部走兜底）
            const graph::mtl::MaterialRecipe *const *recipe_table = nullptr;
            uint32_t recipe_table_count = 0;

            /// 几何文件的基准目录；空则取模型包所在目录
            OSString base_dir;

            /**
             * 装载前门禁：模型包字节到齐、解析产物生成之前，先把场景目录里的
             * 配方（`*.recipe.toml`）与材质数据块投给材质车道，等车道空闲再 `Build()`。
             *
             * 关掉它 ⇒ 配方由调用方自己先同步装好（历史行为）。
             */
            bool preload_scene_recipes = true;

            bool valid = false;
        };

    private:

        StreamHandler *handler          = nullptr;  ///<读盘回调接收者（活到最后一个回调）
        LoadedFileManager *load_manager = nullptr;

        BuildSettings settings;

        /// 每个句柄一份在途状态。句柄本身归 `ModelAssetRegistry`，这里只借指针。
        struct AssetStream
        {
            graph::ModelAsset *asset = nullptr;

            graph::ModelStreamingRequest request;   ///<阶段 A：读盘 + 持字节

            bool plan_built    = false;             ///<计划单已生成并登记到句柄
            bool material_fanout = false;           ///<配方已扇出给材质车道（一次性）
            bool content_built = false;             ///<解析产物已生成
            bool settled       = false;             ///<子项已全部结算（或无需结算）
        };

        ValueArray<AssetStream *> streams;          ///<与 registry 句柄一一对应（拥有）

    private:

        bool EnsureManagers();
        void AcquireOneFrame();
        void Advance();

        /// 把材质车道里已解析完的配方 / 数据块提交进表（微秒级）
        void PumpMaterialLane();

        /// 把场景目录的配方 + 数据块整目录投给材质车道（每个句柄只做一次）
        void FanOutMaterialDeps(AssetStream *stream);

        AssetStream *FindStream(const graph::ModelAsset *asset)const;
        AssetStream *FindStreamByRequest(uint64 request_id)const;
        AssetStream *EnsureStream(graph::ModelAsset *asset);

        /**
         * @brief 丢掉注册表里已经不存在的句柄。
         *
         * `ModelAssetRegistry::Clear()` 会直接析构句柄，而本系统只借指针。
         * 不清理的话下一帧就会拿悬垂指针去查状态 —— 这种崩溃离出事点很远，
         * 极难定位，所以每帧开头先对一遍表。
         */
        void PruneStreams();

        /// 推进单个句柄；返回 false 表示本帧不再需要关注它
        bool AdvanceOne(AssetStream *stream);

        /// 解析产物里有多少图元、其中几何已就绪多少、永久失败多少
        void CountPrimitives(const graph::ModelAssetContent &content,
                             int &total, int &ready, int &failed)const;

    public:

        ModelStreamingSystem();
        ~ModelStreamingSystem() override;

        void Update(float delta_time) override;
        void Shutdown() override;

    public:     // 外部设定

        /**
         * @brief 设定解析模型包所需的几何顶点格式与材质配方。
         *
         * 必须在请求场景之前调用；同一进程内多份约定共存时，以最后一次为准。
         */
        void SetBuildSettings(const graph::GeometryVertexFormat &gvf,
                              const graph::mtl::MaterialRecipe *default_recipe,
                              const graph::mtl::MaterialRecipe *const *recipe_table,
                              uint32_t recipe_table_count,
                              const OSString &base_dir = OSString());

        bool HasBuildSettings()const { return settings.valid; }

    public:     // 观察

        /// 在途 / 待办的句柄数
        int GetStreamCount()const { return streams.GetCount(); }

        /// 本系统登记过的子项落定总数（诊断用）
        int GetReadyModelCount()const;
    };//class ModelStreamingSystem
}//namespace hgl::ecs
