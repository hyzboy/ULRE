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
#include <hgl/ecs/core/System.h>
#include <hgl/ecs/support/ComponentPool.h>
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

namespace
{
    bool read_text_file(const char *path, std::string &out)
    {
        std::ifstream fis(path, std::ios::binary);

        if (!fis)
            return false;

        out.assign(std::istreambuf_iterator<char>(fis), std::istreambuf_iterator<char>());
        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestSystemReplaceAndPoolLiveness"));

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

        std::vector<std::shared_ptr<System>> systems;

        ctx.GetSystemsByElementType(element_type.c_str(), systems);

        if (systems.size() != 1 || systems[0].get() != old_system.get())
        {
            GLogError(u8"S1 Failed: 首注册后列表异常（size=%zu）", systems.size());
            return 1;
        }

        auto new_system = ctx.RegisterSystem<LineCollectSystem>();      // 替换同类型系统

        ctx.GetSystemsByElementType(element_type.c_str(), systems);

        if (systems.size() != 1)
        {
            GLogError(u8"S1 Failed: 替换后 element-type 列表仍有 %zu 个实例（应为 1 —— 旧实例没被摘掉，"
                      u8"组开关/相位区间会把僵尸算进去）", systems.size());
            return 1;
        }

        if (systems[0].get() != new_system.get())
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

        if (!read_text_file("inc/hgl/ecs/support/ComponentPool.h", src))
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
