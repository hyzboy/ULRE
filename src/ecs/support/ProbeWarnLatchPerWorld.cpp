// ProbeWarnLatchPerWorld.cpp —— 批 1.3 C5 判据探针：一次性告警闩必须是**每实例**的
//
// 背景：C5 把 6 处函数内 `static bool` 告警闩改成实例成员（进程级 static ⇒ 第一个世界报过之后，
//       第二个世界（如离屏世界）的同类配置错误被永久静默，排查时看不见）。
//
// 可执行判据取 GPUCullSystem 的契约告警「切片起点未 4 行对齐」（`SetBatchLayout` 的 row_base % 4 != 0）：
//   · 修复前（进程级 static）：只有**第一个**实例报 ⇒ 输出 1 行
//   · 修复后（每实例）        ：两个实例各报一次 ⇒ 输出 **2** 行；同一实例重复触发仍只报 1 次
//
// 判据（外部，本文件是探针而非断言式用例 —— 闩的行为只能从**日志行数**观察，
//       公开 API 里没有查询口，也不该为测试加 API）：
//
//   ./build/out/<cfg>/ProbeWarnLatchPerWorld.exe 2>&1 | grep -c "切片起点未 4 行对齐"   ⇒  期望 2
//
#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

int main()
{
    hgl::logger::InitLogger(OS_TEXT("ProbeWarnLatchPerWorld"));

    {
        GPUCullSystem world_a;      // 世界 A 的 cull 系统
        GPUCullSystem world_b;      // 世界 B 的 cull 系统

        // 未 4 行对齐的切片起点（契约 §9 M4 违反）——两个实例各触发一次
        world_a.SetBatchLayout(0u, 0u, 1u, 8u, 0u, 64u);
        world_b.SetBatchLayout(0u, 0u, 1u, 8u, 0u, 64u);

        // 同一实例再触发（换批号、同样未对齐）：闩仍是"每实例只报一次"——
        // 这一条**不增加**行数，用来确认"每实例"没有退化成"每次调用都报"。
        world_a.SetBatchLayout(0u, 1u, 3u, 8u, 0u, 64u);
    }

    // 摘要行**故意不复述告警原文**：判据是 grep 那句话的行数，摘要若含同串会把计数污染成 3。
    GLogInfo("[ProbeWarnLatchPerWorld] done —— 上面应恰有 2 行 M4 告警（每实例 1 行；同实例重复触发不增行）");

    return 0;
}
