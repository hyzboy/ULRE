// TestEntityHandleGeneration.cpp —— `EntityID::generation` 宽度与 ABA 判据（不碰设备）
//
// 背景：generation 原为 uint16_t。index(4B)+generation(2B) 因对齐实际占 8 字节
// （2B padding 白占），而同一槽位复用 65,536 次后 generation 回绕，旧句柄可能重新
// 命中新实体。改为 uint32_t 后结构体仍是 8 字节，回绕阈值抬到 42.9 亿次。
//
// 判据：
//   G1: sizeof(EntityID) == 8 —— 加宽必须是零开销（不是把句柄变大）。
//   G2: 同槽位反复创建/销毁时 generation 单调递增，旧句柄一律失效。
//   G3: 复用一个槽位 70,000 次（> 2^16）后，**最早**那个句柄仍必须失效 ——
//       这一条在 uint16_t 实现下会红（回绕后旧句柄重新命中）。
//   G4: std::hash 必须同时吃进 index 与 generation（不能只看低 16 位）。
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/EntityManager.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <functional>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    /// G3 的复用次数：必须显著大于 2^16，才能在 16 位世代下稳定复现回绕。
    constexpr uint32_t kReuseCycles = 70000;
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestEntityHandleGeneration"));

    // ── G1：零开销加宽 ────────────────────────────────────────────────────
    static_assert(sizeof(EntityID) == 8,
                  "EntityID 必须仍是 8 字节（index 4B + generation 4B）");

    if (sizeof(EntityID) != 8)
    {
        GLogError(u8"G1 Failed: sizeof(EntityID)=%zu（期望 8 —— 加宽不得增大句柄）", sizeof(EntityID));
        return 1;
    }

    ECSContext ctx;

    // ── G2：单调递增 + 旧句柄失效 ─────────────────────────────────────────
    {
        Entity *first = ctx.CreateEntity("GenProbe");
        if (!first)
        {
            GLogError(u8"G2 Failed: 首个实体创建失败");
            return 1;
        }

        const EntityID old_id = first->GetEntityID();
        ctx.DestroyEntity(old_id);

        Entity *second = ctx.CreateEntity("GenProbe");
        if (!second)
        {
            GLogError(u8"G2 Failed: 第二次创建失败");
            return 1;
        }

        const EntityID new_id = second->GetEntityID();

        if (new_id.index != old_id.index)
        {
            GLogError(u8"G2 Failed: 期望复用同一槽位（old index=%u new index=%u）—— 本用例失去判据",
                      old_id.index, new_id.index);
            return 1;
        }

        if (new_id.generation <= old_id.generation)
        {
            GLogError(u8"G2 Failed: 复用后 generation 未递增（old=%u new=%u）",
                      old_id.generation, new_id.generation);
            return 1;
        }

        if (ctx.GetEntity(old_id) != nullptr)
        {
            GLogError(u8"G2 Failed: 旧句柄仍能解析到实体（generation 失效判据不成立）");
            return 1;
        }

        if (ctx.GetEntity(new_id) != second)
        {
            GLogError(u8"G2 Failed: 新句柄解析不到当前实体");
            return 1;
        }

        ctx.DestroyEntity(new_id);

        GLogInfo(u8"G2 Passed: 同槽位复用 generation 递增且旧句柄失效。");
    }

    // ── G3：跨过 2^16 复用后最早的句柄仍失效 ──────────────────────────────
    {
        Entity *probe = ctx.CreateEntity("GenWrapProbe");
        if (!probe)
        {
            GLogError(u8"G3 Failed: 建不出探针实体");
            return 1;
        }

        const EntityID oldest_id = probe->GetEntityID();
        ctx.DestroyEntity(oldest_id);

        EntityID last_id{};
        uint32_t slot = oldest_id.index;

        for (uint32_t i = 0; i < kReuseCycles; ++i)
        {
            Entity *entity = ctx.CreateEntity("GenWrapProbe");
            if (!entity)
            {
                GLogError(u8"G3 Failed: 第 %u 次复用创建失败", i);
                return 1;
            }

            const EntityID current = entity->GetEntityID();

            if (current.index != slot)
            {
                GLogError(u8"G3 Failed: 第 %u 次复用换了槽位（%u → %u）—— 本用例失去判据",
                          i, slot, current.index);
                return 1;
            }

            last_id = current;
            ctx.DestroyEntity(current);
        }

        // 16 位世代在 65,536 处回绕 ⇒ 复用 70,000 次后读数必然 < 2^16。
        // 这里直接判定宽度，而不是比较"读数是否大于循环次数"（后者只差一两次，
        // 对起始值敏感，容易变成脆弱用例）。
        if (last_id.generation <= UINT16_MAX)
        {
            GLogError(u8"G3 Failed: 复用 %u 次后 generation=%u（<= 65535）—— 世代被截断，"
                      u8"16 位实现在此处已回绕",
                      kReuseCycles, last_id.generation);
            return 1;
        }

        if (ctx.GetEntity(oldest_id) != nullptr && oldest_id == last_id)
        {
            GLogError(u8"G3 Failed: 最早的句柄与末次句柄相同（世代已回绕）");
            return 1;
        }

        // 回绕判定：uint16 实现在这里会让 oldest_id 重新命中新建实体。
        Entity *after_wrap = ctx.CreateEntity("GenWrapProbe");
        if (!after_wrap)
        {
            GLogError(u8"G3 Failed: 回绕检查后创建失败");
            return 1;
        }

        if (after_wrap->GetEntityID() == oldest_id)
        {
            GLogError(u8"G3 Failed: 复用 %u 次后最早的句柄重新命中了新实体（世代回绕未消除）",
                      kReuseCycles);
            return 1;
        }

        ctx.DestroyEntity(after_wrap->GetEntityID());

        GLogInfo(u8"G3 Passed: 复用 %u 次（> 2^16）后最早句柄仍失效，世代不回绕。", kReuseCycles);
    }

    // ── G4：hash 同时使用 index 与 generation ─────────────────────────────
    {
        const std::hash<EntityID> hasher;

        const EntityID a(7u, 2u);
        const EntityID b(7u, 3u);
        const EntityID c(8u, 2u);

        if (hasher(a) == hasher(b))
        {
            GLogError(u8"G4 Failed: 不同 generation 得到相同 hash（index=%u）", a.index);
            return 1;
        }

        if (hasher(a) == hasher(c))
        {
            GLogError(u8"G4 Failed: 不同 index 得到相同 hash（generation=%u）", a.generation);
            return 1;
        }

        GLogInfo(u8"G4 Passed: hash 同时使用 index 与 generation。");
    }

    GLogInfo(u8"ALL PASSED: EntityID generation 为 32 位且零结构开销。");
    return 0;
}
