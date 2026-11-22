#pragma once

#include<hgl/CoreType.h>
#include<hgl/mtl/DebugViewMode.h>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * 调试视图兼容入口。绘制在独立离屏目标，不修改作者材质或主帧。
     * 通过抓帧目标列表取得结果；支持范围及不支持的材质见诊断文档。
     */
    class DebugViewController
    {
    public:
        /// 任意线程：登记期望模式。返回 false = 模式未知（未做任何改动）。
        static bool RequestMode(graph::mtl::DebugViewMode mode);

        /// 任意线程：当前登记的模式。
        static graph::mtl::DebugViewMode GetRequestedMode();

        /// 最近成功提交的离屏视图模式。
        static graph::mtl::DebugViewMode GetAppliedMode();

        /// 兼容旧调用：不再修改世界；离屏绘制由帧调度自动执行，恒返回 false。
        static bool Apply(ECSContext *world);

        /// 世界销毁前释放其离屏目标。
        static void Reset(ECSContext *world);

        /// 最近成功提交的视图包含的实体数量。
        static uint32 GetTrackedCount();
    };
}//namespace hgl::ecs
