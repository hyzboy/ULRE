// TestSystemReplaceAndPoolLiveness.cpp —— §1 无争议补充项 S1 / S2 的判据用例
//
// S1：系统替换必须按**键（类型）**摘旧实例 —— 同类型注册两次后，`systems_by_element_type`
//     里只应留新实例；组开关（`SetSystemGroupEnabled`）不得再碰被替换掉的旧实例。
// S2：裸行池 `Row()` 必须验活跃（`DestroyRow` 之后返回 nullptr，与 `SharedComponentPool` 语义一致）；
//     且 `DestroyRowAt` 必须**先摘宿主再析构**（原顺序是对已结束生命周期的对象写基类子对象）。
//
// 判据（红的样子）：S1 红 = 「替换后 element-type 列表有 2 个实例」或「组开关改到了旧实例」；
//                  S2 红 = 「DestroyRow 后 Row() 仍返回非空（悬垂指针）」；
//                  S2-b 红 = DestroyRowAt 里 SetOwner 出现在 ~T() 之后。
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/system/System.h>
#include <hgl/ecs/core/system/SystemOrderPlanner.h>
#include <hgl/ecs/support/storage/ComponentPool.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/support/line/LineCollectSystem.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace hgl;
using namespace hgl::ecs;

namespace hgl::ecs
{
    struct RenderPassSetupTestAccess
    {
        static void Activate(ECSContext &context) { context.active = true; }
    };
}

namespace
{
    ValueArray<int> scheduled_updates;

    class LateProbeSystem final : public System
    {
    public:
        LateProbeSystem() : System("LateProbeSystem") { SetExecutionPhase(ExecutionPhase::TickTransform); }
        void Update(float) override { scheduled_updates.Add(3); }
    };

    class DependentProbeSystem final : public System
    {
    public:
        DependentProbeSystem() : System("DependentProbeSystem")
        {
            SetExecutionPhase(ExecutionPhase::TickInput);
            AddDependency<LateProbeSystem>();
        }
        void Update(float) override { scheduled_updates.Add(1); }
    };

    class MiddleProbeSystem final : public System
    {
    public:
        MiddleProbeSystem() : System("MiddleProbeSystem") { SetExecutionPhase(ExecutionPhase::TickPhysics); }
        void Update(float) override { scheduled_updates.Add(2); }
    };

    bool read_text_file(const char *path, std::string &out)
    {
        std::ifstream fis(path, std::ios::binary);

        if (!fis)
            return false;

        out.assign(std::istreambuf_iterator<char>(fis), std::istreambuf_iterator<char>());
        return true;
    }

    /// S1-b 探针系统：替换时旧实例必须收到一次 Shutdown（否则它持有的
    /// GPU 缓冲/纹理/监听器在 sys_map 覆盖后永远没人释放）。
    class ReplaceProbeSystem final : public System
    {
    public:

        int shutdown_count = 0;

        ReplaceProbeSystem() : System("ReplaceProbeSystem")
        {
            SetExecutionPhase(ExecutionPhase::TickTransform);
        }

        void Shutdown() override
        {
            ++shutdown_count;
        }
    };
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestSystemReplaceAndPoolLiveness"));

    {
        ComponentPoolRegistry registry;
        if (registry.Get<GeometryData>() || registry.Get(ComponentType::None))
        {
            GLogError(u8"Component pools must be created lazily");
            return 1;
        }
        auto *pool = registry.Ensure<GeometryData>();
        if (!pool || registry.Ensure<GeometryData>() != pool || !pool->Create(nullptr))
        {
            GLogError(u8"Component pool registration failed");
            return 1;
        }
        registry.Clear();
        if (registry.Get<GeometryData>() || registry.Ensure<GeometryData>()->GetActiveCount() != 0)
        {
            GLogError(u8"Component pool registry did not clear its slots and rows");
            return 1;
        }
    }

    {
        SystemScheduleState schedule;
        schedule.AddDependency(false, 1, 2);
        schedule.AddDependency(false, 1, 2);
        schedule.AddDependency(false, 1, 1);
        schedule.AddDependency(true, 1, 3);
        const auto *tick = schedule.Dependencies(false).GetValuePointer(1);
        const auto *render = schedule.Dependencies(true).GetValuePointer(1);
        if (!tick || tick->GetCount() != 1 || (*tick)[0] != 2
            || !render || render->GetCount() != 1 || (*render)[0] != 3
            || !schedule.Dirty(false) || !schedule.Dirty(true)
            || schedule.NextInsertionOrder() != 1 || schedule.NextInsertionOrder() != 2)
        {
            GLogError(u8"System schedule dependency or insertion state is incorrect");
            return 1;
        }
        schedule.RemoveDependencies(false, 1);
        if (schedule.Dependencies(false).GetValuePointer(1)
            || !schedule.Dependencies(true).GetValuePointer(1))
        {
            GLogError(u8"System schedule removed dependencies from the wrong phase");
            return 1;
        }
    }

    {
        SystemOrderPlanner planner;
        planner.AddSystem(1, 0, 0, 1);
        planner.AddSystem(2, 0, 0, 2);
        planner.AddSystem(3, 1, 0, 3);
        if (planner.AddDependency(1, 3) != SystemOrderPlanner::DependencyResult::Added
            || planner.AddDependency(1, 99) != SystemOrderPlanner::DependencyResult::MissingDependency
            || planner.AddDependency(99, 1) != SystemOrderPlanner::DependencyResult::MissingDependent)
        {
            GLogError(u8"System ordering dependency resolution failed");
            return 1;
        }
        ValueArray<int> order;
        if (!planner.Build(order) || order.GetCount() != 3
            || order[0] != 1 || order[1] != 2 || order[2] != 0)
        {
            GLogError(u8"System ordering did not honor dependencies and phase ordering");
            return 1;
        }
        planner.AddDependency(3, 1);
        if (planner.Build(order) || order.GetCount() != 3
            || order[0] != 0 || order[1] != 1 || order[2] != 2)
        {
            GLogError(u8"System ordering did not fall back on a dependency cycle");
            return 1;
        }
    }

    {
        ECSContext context;
        context.RegisterSystem<DependentProbeSystem>();
        context.RegisterSystem<MiddleProbeSystem>();
        context.RegisterSystem<LateProbeSystem>();
        RenderPassSetupTestAccess::Activate(context);
        scheduled_updates.Clear();
        context.Tick(0);
        if (scheduled_updates.GetCount() != 3 || scheduled_updates[0] != 2
            || scheduled_updates[1] != 3 || scheduled_updates[2] != 1)
        {
            GLogError(u8"ECS tick did not execute systems in planned dependency order");
            return 1;
        }
    }

    {
        SystemGroupActivity activity;
        activity.Add(SystemGroupID::Line);
        activity.Add(SystemGroupID::Line);
        activity.Add(SystemGroupID::Text);
        activity.MarkInstalled(SystemGroupID::Line);
        if (activity.Remove(SystemGroupID::Line)
            || !activity.Remove(SystemGroupID::Line)
            || !activity.Remove(SystemGroupID::Line)
            || !activity.IsInstalled(SystemGroupID::Line)
            || activity.IsInstalled(SystemGroupID::Text))
        {
            GLogError(u8"System group activity counts or installation state are incorrect");
            return 1;
        }
        activity.Reset();
        if (activity.IsInstalled(SystemGroupID::Line) || !activity.Remove(SystemGroupID::Text))
        {
            GLogError(u8"System group activity reset did not clear all groups");
            return 1;
        }
    }

    // ── S1：同类型系统替换 ──────────────────────────────────────────────────
    {
        ECSContext ctx;

        auto old_system = ctx.RegisterSystem<LineCollectSystem>();

        if (!old_system)
        {
            GLogError(u8"S1 Failed: 首个系统注册失败");
            return 1;
        }

        const AnsiString element_type = old_system->GetRenderElementType();

        if (element_type.IsEmpty())
        {
            GLogError(u8"S1 Failed: 被测系统未声明 render element type（本用例失去判据）");
            return 1;
        }

        const PtrArray<std::shared_ptr<System>> *systems =
            ctx.GetSystemRegistry().FindByElementType(element_type);

        if (!systems || systems->GetCount() != 1 || (*systems)[0].get() != old_system.get())
        {
            GLogError(u8"S1 Failed: 首注册后列表异常");
            return 1;
        }

        auto new_system = ctx.RegisterSystem<LineCollectSystem>();      // 替换同类型系统

        systems = ctx.GetSystemRegistry().FindByElementType(element_type);

        if (!systems || systems->GetCount() != 1)
        {
            GLogError(u8"S1 Failed: 替换后 element-type 列表不再只包含 1 个实例");
            return 1;
        }

        if ((*systems)[0].get() != new_system.get())
        {
            GLogError(u8"S1 Failed: 列表里剩的不是新注册的实例");
            return 1;
        }

        const SystemGroupID group = SystemGroupFromName(element_type.c_str());

        if (group == SystemGroupID::None)
        {
            GLogError(u8"S1 Failed: 元素类型 %s 不是系统组名（本用例依赖两者同名）", element_type.c_str());
            return 1;
        }

        ctx.SetSystemGroupEnabled(group, false);

        if (new_system->IsEnabled())
        {
            GLogError(u8"S1 Failed: 组关闭没有作用到当前实例");
            return 1;
        }

        if (!old_system->IsEnabled())
        {
            GLogError(u8"S1 Failed: 组开关改到了**已被替换**的旧实例（僵尸仍被当成员关掉）");
            return 1;
        }

        ctx.SetSystemGroupEnabled(group, true);

        if (!new_system->IsEnabled())
        {
            GLogError(u8"S1 Failed: 组重新打开没有作用到当前实例");
            return 1;
        }

        GLogInfo(u8"S1 Passed: 系统替换按类型摘旧实例成立（列表只留新实例 / 组开关不再碰到旧实例）。");
    }

    // ── S1-b：替换同类型系统时旧实例必须收到 Shutdown ──────────────────────
    {
        ECSContext ctx;

        auto first = ctx.RegisterSystem<ReplaceProbeSystem>();

        if (!first || first->shutdown_count != 0)
        {
            GLogError(u8"S1-b Failed: 首注册异常（shutdown=%d）", first ? first->shutdown_count : -1);
            return 1;
        }

        // 注册**同类型**的第二个实例 ⇒ 旧实例被摘出世界。
        auto second = ctx.RegisterSystem<ReplaceProbeSystem>();

        if (!second || second.get() == first.get())
        {
            GLogError(u8"S1-b Failed: 同类型替换没有产生新实例");
            return 1;
        }

        if (first->shutdown_count != 1)
        {
            GLogError(u8"S1-b Failed: 被替换的旧实例收到 %d 次 Shutdown（应为 1 —— 覆盖 sys_map "
                      u8"后它已不在世界里，资源永不释放）", first->shutdown_count);
            return 1;
        }

        if (second->shutdown_count != 0)
        {
            GLogError(u8"S1-b Failed: 新实例在注册期就被 Shutdown 了（%d 次）", second->shutdown_count);
            return 1;
        }

        GLogInfo(u8"S1-b Passed: 同类型替换会先 Shutdown 旧实例，新实例不受影响。");
    }

    // ── S2-a：两个池的 Row() 对已释放行都必须返回 nullptr ────────────────────
    {
        ComponentPool<GeometryData> pool;               // 裸行池（块内就地构造）

        const uint32_t handle = pool.Create(nullptr);

        if (handle == 0 || pool.Row(handle) == nullptr)
        {
            GLogError(u8"S2 Failed: 新建行后 Row() 应非空（handle=%u）", handle);
            return 2;
        }

        pool.DestroyRow(handle);

        if (pool.Row(handle) != nullptr)
        {
            GLogError(u8"S2 Failed: DestroyRow 后 Row(%u) 仍返回非空（指向已析构行的悬垂指针）", handle);
            return 2;
        }

        pool.DestroyRow(handle);                        // 重复释放仍是 no-op

        if (pool.GetActiveCount() != 0)
        {
            GLogError(u8"S2 Failed: 重复释放改变了活跃计数（%u）", pool.GetActiveCount());
            return 2;
        }

        SharedComponentPool<GeometryData> shared_pool;  // shared_ptr 行池（同一 T ⇒ 语义可直接对比）

        const uint32_t shared_handle = shared_pool.Create(nullptr);

        if (shared_handle == 0 || shared_pool.Row(shared_handle) == nullptr)
        {
            GLogError(u8"S2 Failed: shared 池新建行后 Row() 应非空（handle=%u）", shared_handle);
            return 2;
        }

        shared_pool.DestroyRow(shared_handle);

        if (shared_pool.Row(shared_handle) != nullptr)
        {
            GLogError(u8"S2 Failed: shared 池 DestroyRow 后 Row(%u) 仍返回非空", shared_handle);
            return 2;
        }

        GLogInfo(u8"S2-a Passed: 两个池的 Row() 对已释放行都返回 nullptr（语义对齐）。");
    }

    // ── S2-b：DestroyRowAt 的顺序（源码针 —— 运行期读已析构行同样不安全，故钉顺序）──
    {
        std::string src;

        if (!read_text_file("inc/hgl/ecs/support/storage/ComponentPool.h", src))
        {
            GLogError(u8"S2-b Failed: cannot open ComponentPool.h (run from repo root)");
            return 3;
        }

        const size_t owner_pos = src.find("static_cast<ComponentRow *>(row)->SetOwner(nullptr);");
        const size_t dtor_pos  = src.find("row->~T();");

        if (owner_pos == std::string::npos || dtor_pos == std::string::npos || owner_pos > dtor_pos)
        {
            GLogError(u8"S2-b Failed: DestroyRowAt 必须**先摘宿主再析构**"
                      u8"（SetOwner 位置=%zu / ~T() 位置=%zu）", owner_pos, dtor_pos);
            return 3;
        }

        GLogInfo(u8"S2-b Passed: DestroyRowAt 顺序成立（先 SetOwner(nullptr) 再 row->~T()）。");
    }

    GLogInfo(u8"=== System replace & pool liveness contracts passed! ===");
    return 0;
}
