// SceneLoadPlan.h — 一个场景「要先装哪些东西」的完整应答
//
// 这是阶段三的收口：把 `AssetLoadGraph`（谁依赖谁）变成一张**可以直接派发**的
// 任务单。任务单只回答顺序与身份，不负责装载；真正的 IO 与显存提交由阶段四的
// `SceneStreamingSystem` 按这张单子级联下发。
//
// ## 为什么要有「计划」这一层
//
// 直接把依赖边丢给流送系统，会立刻遇到两个真实问题：
//   1. **重复请求**：ABeautifulGame 的棋盘上，同一种棋子出现多次，材质与几何
//      在场景包里是同一个条目。若按「每个 primitive 提一次请求」派发，同一份
//      8MB 几何会被并发读多遍、也会被显存里拷贝多份。计划单按**身份去重**，
//      同一节点只出现一次，并带上 `ref_count`（有多少个父节点引用它）供上层
//      决定是否复用句柄。
//   2. **顺序错乱**：纹理与几何必须早于材质 SSBO，材质必须早于场景提交。计划单
//      直接给出「先依赖后使用」的拓扑序，派发方按序推进即可，不必自己再做排序。
//
// ## 与 `hgl::asset::AssetDependencyGraph` 的分工
//
// 见 `AssetLoadGraph.h` 顶部说明。这里复用同一套身份口径：`{类型, 小写去后缀
// 的包内相对路径}`，与 `AssetLoadGraph` / `SceneDependencyExtractor` 完全一致。

#pragma once

#include <hgl/graph/asset/AssetLoadGraph.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    /// 计划单里的一项：一个待装载的资产。
    struct SceneLoadTask
    {
        AssetDepKey key;
        AssetDepKind kind = AssetDepKind::Geometry;

        /// 有多少个父节点直接引用它（>1 表示需要复用句柄，不要重复装载）
        int ref_count = 0;
    };

    /// 场景装载计划单。
    struct SceneLoadPlan
    {
        /// 依赖闭包，**不含**场景自身，按「先依赖后使用」排序
        ValueArray<SceneLoadTask> tasks;

        /// 依赖图是否无环。有环时 tasks 仍然可用（环上节点只出现一次），
        /// 但顺序不再是严格拓扑序 —— 调用方应当告警而不是静默继续。
        bool acyclic = true;

        /// 内嵌在场景包里的几何份数。自足包不产生外部几何依赖（`CountOf(
        /// Geometry)==0`），派发时无需为几何做 IO，但这不等于场景里没有几何。
        int embedded_geometry_count = 0;

        int TotalCount() const { return tasks.GetCount(); }

        /// 统计某一类任务的数量。
        int CountOf(AssetDepKind kind) const;
    };

    /**
     * 生成场景装载计划单。
     *
     * 步骤：
     *   1. 经 `OpenAssetInputStream` 取场景包字节（不解析顶点、不建 GPU 资源）；
     *   2. 解析 SCN2 包头与载荷目录，抽出材质 / 几何依赖；
     *   3. 按材质名在场景目录里枚举 `*.recipe.toml`，解析出纹理依赖；
     *   4. 求解依赖闭包，按拓扑序落成任务单。
     *
     * 全程只读字节：不碰 `GraphicsContext`，不要求设备已创建。因此在设备初始化
     * 之前就能知道「这个场景要占多少显存、要读多少文件」。
     *
     * @param scene_ref   场景引用（`pack(res):xxx/yyy.Scene` 或裸物理路径）
     * @param out         计划单（会先被清空）
     * @param out_graph   可为 nullptr；非空时写入本次构建的依赖图，供诊断
     * @param out_error   失败原因（可为 nullptr）
     * @return 场景包读取 / 解析成功返回 true。**配方缺失不算失败** —— 配方缺失
     *         会在装载期表现为「材质回退默认值」，但几何与纹理依赖仍然有效，
     *         提前判失败反而会让整个场景打不开。
     */
    bool BuildSceneLoadPlan(const AnsiString &scene_ref,
                            SceneLoadPlan &out,
                            AssetLoadGraph *out_graph = nullptr,
                            AnsiString *out_error = nullptr);

    /**
     * @brief 同上，但**复用调用方已经读到的字节**。
     *
     * 异步流送里场景包已经由 IO 线程读进内存（`SceneStreamingRequest` 持有），
     * 此时再调 `BuildSceneLoadPlan` 会把同一个大包重复读一遍。本入口只做
     * 「解析 + 求解闭包」，不碰 IO —— 除配方文件外（配方很小，且必须读到
     * 纹理依赖才能算出闭包）。
     *
     * `pack_name` 只用于日志与 `base_dir` 推导的失败诊断，不参与解析。
     *
     * @param scene_ref  场景引用（与 `BuildSceneLoadPlan` 同口径，决定身份与 base_dir）
     * @param pack_bytes 场景包完整字节
     * @param pack_size  字节数
     */
    bool BuildSceneLoadPlanFromBytes(const AnsiString &scene_ref,
                                     const void *pack_bytes,
                                     const uint32 pack_size,
                                     SceneLoadPlan &out,
                                     AssetLoadGraph *out_graph = nullptr,
                                     AnsiString *out_error = nullptr);

    /// 诊断用：把计划单打到日志（按类型汇总 + 逐项）。
    void DumpSceneLoadPlan(const AnsiString &scene_ref, const SceneLoadPlan &plan);
}//namespace hgl::graph
