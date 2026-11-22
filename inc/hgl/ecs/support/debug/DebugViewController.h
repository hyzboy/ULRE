#pragma once

#include<hgl/CoreType.h>
#include<hgl/mtl/DebugViewMode.h>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * DebugViewController — 调试视图的世界级开关（渲染自省阶段二）。
     *
     * 设计见 `doc/debug-analysis-and-view-modes.md` §3.4。要点：
     *
     *   · **不新增渲染管线、不新增离屏 RT**：调试视图就是把现有几何用
     *     `MaterialDomain::DebugAttribute` 的配方再画一遍，复用全部间接绘制 /
     *     实例化 / 顶点 ABI；
     *   · 开关 = 逐实体的 `MaterialData::SetRecipe()` 覆盖。首次接管某实体时把
     *     「原配方（或原本没有覆盖这一事实）」记进旁表，`Off` 时逐条还原；
     *   · 关闭状态**零开销**：`Apply()` 只比较一次模式，未变即返回 false。
     *
     * 线程模型：`RequestMode()` 可从任意线程调用（命名管道线程），`Apply()` 只在
     * 渲染线程的收集阶段之前调用。两者之间只有一把极短的互斥锁。
     *
     * 已知边界（有意为之）：只覆盖**有 `MaterialData` 授权记录**的图元。
     * 天空 / UI / 引擎调试线等内部材质不参与——它们不是被诊断的资产材质。
     */
    class DebugViewController
    {
    public:
        /// 任意线程：登记期望模式。返回 false = 模式未知（未做任何改动）。
        static bool RequestMode(graph::mtl::DebugViewMode mode);

        /// 任意线程：当前登记的模式（下一次 `Apply()` 生效）。
        static graph::mtl::DebugViewMode GetRequestedMode();

        /// 渲染线程：当前**实际生效**的模式（已应用到材质上的那一份）。
        static graph::mtl::DebugViewMode GetAppliedMode();

        /// 渲染线程：把登记的模式应用到世界。返回 true = 本次发生了切换。
        /// 模式未变 / world 为空 ⇒ 返回 false 且不做任何遍历。
        static bool Apply(ECSContext *world);

        /// 渲染线程：还原全部被接管的实体并清空记录（世界销毁前调用）。
        static void Reset(ECSContext *world);

        /// 当前被接管的实体数量；诊断用。
        static uint32 GetTrackedCount();
    };
}//namespace hgl::ecs
