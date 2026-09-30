#include <hgl/ecs/support/BoundingBoxDataStorage.h>
#include <hgl/ecs/support/BoundingBoxAccessor.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <glm/glm.hpp>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    constexpr float kEps = 1e-5f;

    bool Near(const glm::vec3 &a, const glm::vec3 &b)
    {
        return glm::distance(a,b) <= kEps;
    }

    math::AABB MakeBox(const glm::vec3 &min_v,const glm::vec3 &max_v)
    {
        math::AABB box;
        box.SetMinMax(min_v,max_v);
        return box;
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // 没有这一行时本测试的所有 GLog（含失败原因）都无处可去——静默通过/静默失败
    hgl::logger::InitLogger(OS_TEXT("TestBoundingBoxStorage"));

    GLogInfo(u8"=== Testing BoundingBoxDataStorage (SoA) + BoundingBoxAccessor (值类型句柄) ===");

    const math::AABB box_a = MakeBox(glm::vec3(-1.0f,-2.0f,-3.0f),glm::vec3(3.0f,4.0f,5.0f));   // center (1,1,1)  extents (2,3,4)
    const math::AABB box_b = MakeBox(glm::vec3(10.0f,10.0f,10.0f),glm::vec3(12.0f,12.0f,12.0f));

    // ─────────────────────────────────────────────────────────────
    // Test 1: 存储层——分配/世代/派生 center-extents/owner 反查/释放清理
    // ─────────────────────────────────────────────────────────────
    {
        BoundingBoxDataStorage storage;

        const auto id0 = storage.Allocate();
        const auto id1 = storage.Allocate();

        if (storage.GetCount() != 2u)
        {
            GLogError(u8"Test 1 Failed: GetCount()=%u，期望 2",storage.GetCount());
            return 1;
        }

        if (!storage.IsAllocated(id0) || storage.GetGeneration(id0) != 1)
        {
            GLogError(u8"Test 1 Failed: 新行应已分配且世代 = 1（正奇数 = 活）");
            return 1;
        }

        if (storage.IsAllocated(99) || storage.GetGeneration(99) != 0)
        {
            GLogError(u8"Test 1 Failed: 越界行的世代应为 0");
            return 1;
        }

        if (!(id0 != id1))
        {
            GLogError(u8"Test 1 Failed: 行不复用，两次分配必须给出不同行号");
            return 1;
        }

        // center / extents 由 AABB 派生
        storage.SetLocalBounds(id0,box_a);

        if (!Near(storage.GetCenter(id0),glm::vec3(1.0f,1.0f,1.0f)))
        {
            GLogError(u8"Test 1 Failed: GetCenter 派生值错");
            return 1;
        }

        if (!Near(storage.GetExtents(id0),glm::vec3(2.0f,3.0f,4.0f)))
        {
            GLogError(u8"Test 1 Failed: GetExtents 派生值错");
            return 1;
        }

        // world_bounds：未设时 HasWorldBounds=false
        if (storage.HasWorldBounds(id0))
        {
            GLogError(u8"Test 1 Failed: 未设 world_bounds 时 HasWorldBounds 应为 false");
            return 1;
        }

        storage.SetWorldBounds(id0,box_a);

        if (!storage.HasWorldBounds(id0))
        {
            GLogError(u8"Test 1 Failed: 设过 world_bounds 后 HasWorldBounds 应为 true");
            return 1;
        }

        // owner + 反向索引
        const EntityID owner(7,1);
        storage.SetOwner(id0,owner);

        if (storage.GetOwner(id0) != owner)
        {
            GLogError(u8"Test 1 Failed: GetOwner 不匹配");
            return 1;
        }

        if (storage.FindByOwner(owner) != id0)
        {
            GLogError(u8"Test 1 Failed: FindByOwner 未找到刚登记的行");
            return 1;
        }

        // 释放：世代归 0 + owner/反查/world_valid 全部清理
        storage.Deallocate(id0);

        if (storage.GetGeneration(id0) != 0 || storage.IsAllocated(id0))
        {
            GLogError(u8"Test 1 Failed: 释放后世代未归 0（行未失效）");
            return 1;
        }

        if (storage.HasWorldBounds(id0))
        {
            GLogError(u8"Test 1 Failed: 释放后 HasWorldBounds 仍为 true（world_valid 未清）");
            return 1;
        }

        if (storage.GetOwner(id0).IsValid() || storage.FindByOwner(owner) != BoundingBoxDataStorage::INVALID_HANDLE)
        {
            GLogError(u8"Test 1 Failed: 释放后 owner / 反查项未清理");
            return 1;
        }

        // 行不复用：释放后再分配拿到的是新行号，老行仍是死的
        const auto id2 = storage.Allocate();

        if (id2 == id0 || storage.GetCount() != 3u)
        {
            GLogError(u8"Test 1 Failed: 行不复用契约被破坏（应取尾部新行）");
            return 1;
        }

        GLogInfo(u8"Test 1 Passed: 分配/世代/派生 center-extents/owner 反查/释放清理。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 句柄失效契约（与 TransformAccessor 同约定）
    //   · 活行的句柄有效
    //   · 行释放后，**旧句柄立刻失效**（悬垂写入窗口为零）
    //   · **死行上的新句柄也必须无效**
    //   · 无效句柄的写入被忽略，且不得影响存储
    // ─────────────────────────────────────────────────────────────
    {
        BoundingBoxDataStorage storage;

        const auto id = storage.Allocate();
        BoundingBoxAccessor live(&storage,id);

        if (!live.IsValid())
        {
            GLogError(u8"Test 2 Failed: 活行的句柄无效");
            return 2;
        }

        live.SetLocalBounds(box_a);

        if (!Near(live.GetCenter(),glm::vec3(1.0f,1.0f,1.0f)) || !Near(live.GetExtents(),glm::vec3(2.0f,3.0f,4.0f)))
        {
            GLogError(u8"Test 2 Failed: accessor 写入/读取未直落存储");
            return 2;
        }

        live.SetWorldBounds(box_a);

        if (!live.HasWorldBounds())
        {
            GLogError(u8"Test 2 Failed: SetWorldBounds 后 HasWorldBounds 应为 true");
            return 2;
        }

        // 空句柄：全部读取安全、全部写入被忽略
        const BoundingBoxAccessor empty;

        if (empty.IsValid() || empty.HasWorldBounds())
        {
            GLogError(u8"Test 2 Failed: 默认构造的空句柄被判为有效");
            return 2;
        }

        empty.SetLocalBounds(box_b);
        empty.SetWorldBounds(box_b);

        {
            const math::AABB still = storage.GetLocalBounds(id);

            if (!Near(glm::vec3(still.GetMin()),glm::vec3(-1.0f,-2.0f,-3.0f)))
            {
                GLogError(u8"Test 2 Failed: 空句柄的写入影响了存储");
                return 2;
            }
        }

        // 释放 ⇒ 旧句柄立刻失效
        const EntityID owner(3,1);
        storage.SetOwner(id,owner);
        storage.Deallocate(id);

        if (live.IsValid())
        {
            GLogError(u8"Test 2 Failed: 行释放后旧句柄仍报有效（悬垂写入窗口）");
            return 2;
        }

        live.SetLocalBounds(box_b);     // 失效写入必须被忽略

        {
            const math::AABB still = storage.GetLocalBounds(id);

            if (!Near(glm::vec3(still.GetMin()),glm::vec3(-1.0f,-2.0f,-3.0f)))
            {
                GLogError(u8"Test 2 Failed: 失效句柄的写入影响了存储");
                return 2;
            }
        }

        // 死行上的**新**句柄同样必须无效（否则"拿到已释放行"这条路会静默生效）
        const BoundingBoxAccessor fresh(&storage,id);

        if (fresh.IsValid() || fresh.HasWorldBounds())
        {
            GLogError(u8"Test 2 Failed: 死行上的新句柄被判定为有效（世代 0 未生效）");
            return 2;
        }

        fresh.SetWorldBounds(box_b);

        if (storage.HasWorldBounds(id))
        {
            GLogError(u8"Test 2 Failed: 死行上的写入生效了（世代校验被绕过）");
            return 2;
        }

        GLogInfo(u8"Test 2 Passed: 句柄失效契约（活柄有效 / 释放即失效 / 死行新柄也失效 / 无效写入被忽略）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: ECSContext 接入（世界私有存储 + entity 级 API）
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext context;

        if (!context.GetBoundingBoxStorage())
        {
            GLogError(u8"Test 3 Failed: 世界未持有包围盒存储");
            return 3;
        }

        auto ent = context.CreateEntity<Entity>("BBoxProbe");

        if (!ent)
        {
            GLogError(u8"Test 3 Failed: 建实体失败");
            return 3;
        }

        const EntityID eid = ent->GetEntityID();

        if (context.GetBoundingBoxByEntity(eid).IsValid())
        {
            GLogError(u8"Test 3 Failed: 尚未创建包围盒时应返回无效句柄");
            return 3;
        }

        const BoundingBoxAccessor a = context.GetOrCreateBoundingBox(eid);

        if (!a.IsValid())
        {
            GLogError(u8"Test 3 Failed: GetOrCreateBoundingBox 未建出行");
            return 3;
        }

        const BoundingBoxAccessor b = context.GetOrCreateBoundingBox(eid);

        if (!b.IsValid() || b.GetID() != a.GetID())
        {
            GLogError(u8"Test 3 Failed: GetOrCreateBoundingBox 非幂等（同实体出现两行）");
            return 3;
        }

        if (a.GetOwner() != eid)
        {
            GLogError(u8"Test 3 Failed: accessor 解析 owner 失败");
            return 3;
        }

        a.SetLocalBounds(box_a);
        a.SetWorldBounds(box_a);

        if (!Near(a.GetCenter(),glm::vec3(1.0f,1.0f,1.0f)) || !a.HasWorldBounds())
        {
            GLogError(u8"Test 3 Failed: 经世界句柄的读写未落存储");
            return 3;
        }

        // 实体销毁 ⇒ 行回收（与变换行同契约：行属于世界，不挂在实体上）
        context.DestroyEntity(eid);

        if (a.IsValid())
        {
            GLogError(u8"Test 3 Failed: 实体销毁后旧句柄仍有效（行未回收）");
            return 3;
        }

        if (context.GetBoundingBoxStorage()->FindByOwner(eid) != BoundingBoxDataStorage::INVALID_HANDLE)
        {
            GLogError(u8"Test 3 Failed: 实体销毁后反查表仍留残行");
            return 3;
        }

        GLogInfo(u8"Test 3 Passed: 世界私有存储 + GetOrCreateBoundingBox 幂等 + DestroyEntity 回收。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: 每行数组同步不变量 + 行字节账目
    // ─────────────────────────────────────────────────────────────
    {
        BoundingBoxDataStorage storage;

        for (uint32_t i = 0; i < 5u; ++i)
            storage.Allocate();

        storage.Deallocate(2);      // 行不复用：死行仍占行号

        if (storage.GetCount() != 5u)
        {
            GLogError(u8"Test 4 Failed: 释放不应改变行数（行不复用）");
            return 4;
        }

        if (const char *mismatch = storage.FindPerRowCountMismatch())
        {
            GLogError(u8"Test 4 Failed: 每行数组 '%s' 元素数与行数不一致——漏在 Allocate/Deallocate 里同步",
                      mismatch);
            return 4;
        }

        uint32_t total = 0;

        for (const BoundingBoxDataStorage::PerRowField &f : storage.GetPerRowFields())
        {
            GLogInfo(u8"    per-row field[%s]: bytes=%u count=%u",
                     f.name,
                     f.bytes,
                     f.count);

            total += f.bytes;
        }

        // 回归护栏：行布局若被改回「存 math::AABB」，每行会立刻从 ~77 B 膨胀回 717 B
        const uint32_t expected_bytes = static_cast<uint32_t>(4*sizeof(glm::vec3) + sizeof(uint8_t) + sizeof(EntityID) + sizeof(uint32_t));

        if (total != expected_bytes)
        {
            GLogError(u8"Test 4 Failed: 每行字节合计 %u ≠ 期望 %u（行布局被改动？）",total,expected_bytes);
            return 4;
        }

        GLogInfo(u8"Test 4 Passed: 每行数组同步（%u 行 × %u 条平行数组 = 每行 %u B；"
                 u8"其中 sizeof(glm::vec3)=%u（local/world 的 min/max 共 4 列 = %u B）、sizeof(math::AABB)=%u —— "
                 u8"AABB 自带 352 B 派生缓存（6 面中心 + 6 平面），行存储只需 min/max 故不存 AABB；"
                 u8"sizeof(EntityID)=%u）。",
                 storage.GetCount(),
                 static_cast<uint32_t>(BoundingBoxDataStorage::PER_ROW_FIELD_COUNT),
                 total,
                 static_cast<uint32_t>(sizeof(glm::vec3)),
                 static_cast<uint32_t>(4*sizeof(glm::vec3)),
                 static_cast<uint32_t>(sizeof(math::AABB)),
                 static_cast<uint32_t>(sizeof(EntityID)));
    }

    GLogInfo(u8"=== All BoundingBoxDataStorage / Accessor tests passed successfully! ===");
    return 0;
}
