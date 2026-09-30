#include <hgl/ecs/support/MaterialRuntimeTable.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

/**
 * A4 材质运行期表（v2 §9.3）
 *
 *   · **共享行 interned**：同授权态（同键）多实体 `Intern` ⇒ 同一行、行数不增、
 *     refcount 累加；键**三维**（program 身份 / 配方身份 / 材质数据行）任一不同都必然
 *     分裂成两行（不串味）；
 *   · **refcount + 世代**：归零回收；行号复用按 `+2` **保持奇数**（0 = 死）；
 *   · **写时分裂（CoW）**：`DetachOwned` 在多处引用时复制出自有行（内容与源行一致、
 *     源行 refcount -1、自有行不入键索引）；独占时就地转自有；已是自有行幂等；
 *   · **每实例 slot**：按实体稀疏存放、幂等取/建；**同行的两个实例各自持自己的
 *     选择器/重试计数/脏标志**（每实例状态绝不进共享行，也不进行键）；
 *   · 预算制：超容量上限 fail-fast（返回 INVALID）且**只告警一次**，不静默扩容。
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestMaterialRuntimeTable"));

    GLogInfo(u8"=== Testing 材质运行期表（A4）===");

    MaterialRuntimeTable table;

    // ─────────────────────────────────────────────────────────────
    // Test 1: 共享行 interned —— 同键两次 Intern ⇒ 同一行、行数不增、refcount 累加
    //   （核心不变量：同授权态的多实体引用同一行）
    // ─────────────────────────────────────────────────────────────
    MaterialRuntimeRowID shared_row = INVALID_MATERIAL_RUNTIME_ROW_ID;
    {
        const MaterialRuntimeKey key{0x1111ull, 0x2222ull, 7u};

        shared_row = table.Intern(key);

        if (shared_row == INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            GLogError(u8"Test 1 Failed: 正常登记不该返回 INVALID");
            return 10;
        }

        if (table.GetCount() != 1 || table.GetSharedRowCount() != 1 || table.GetOwnedRowCount() != 0)
        {
            GLogError(u8"Test 1 Failed: 首次登记后计数异常（活行 %u 共享 %u 自有 %u）",
                      table.GetCount(), table.GetSharedRowCount(), table.GetOwnedRowCount());
            return 10;
        }

        const MaterialRuntimeRowID again = table.Intern(key);

        if (again != shared_row)
        {
            GLogError(u8"Test 1 Failed: 同键两次 Intern 得到不同行（%u != %u）——去重失效",
                      shared_row, again);
            return 10;
        }

        if (table.GetCount() != 1)
        {
            GLogError(u8"Test 1 Failed: 同键重复 Intern 让行数增长（%u）", table.GetCount());
            return 10;
        }

        if (table.GetRefCount(shared_row) != 2)
        {
            GLogError(u8"Test 1 Failed: 两次 Intern 的 refcount 应为 2，实为 %u",
                      table.GetRefCount(shared_row));
            return 10;
        }

        const MaterialRuntimeRow *row = table.Get(shared_row);
        if (!row || row->key.program_build_context != 0x1111ull
                 || row->key.recipe_hash != 0x2222ull
                 || row->key.data_index_row != 7u
                 || row->owned
                 || row->data_index_row != uint32_t(-1))
        {
            GLogError(u8"Test 1 Failed: 行内容/键残缺，或物化标记不是 uint32_t(-1)");
            return 10;
        }

        if (table.GetGeneration(shared_row) != 1)
        {
            GLogError(u8"Test 1 Failed: 新行世代应为 1（正奇数），实为 %u",
                      table.GetGeneration(shared_row));
            return 10;
        }

        if (!table.GetMutable(shared_row) || table.Get(99999u) != nullptr
         || table.Get(INVALID_MATERIAL_RUNTIME_ROW_ID) != nullptr)
        {
            GLogError(u8"Test 1 Failed: 非法行号必须返回空（不能越界取到行）");
            return 10;
        }

        GLogInfo(u8"Test 1 Passed: 同授权态 intern ⇒ 同一行（ID=%u，refcount=%u）。",
                 shared_row, table.GetRefCount(shared_row));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 键三维对等参与身份 —— 任一维不同 ⇒ 不同行
    //   （尤其 data_index：HashMaterialRecipe 不含它，只按 recipe 去重会把
    //     "同配方、不同数据行"的实体并成一行）
    // ─────────────────────────────────────────────────────────────
    MaterialRuntimeRowID other_recipe = INVALID_MATERIAL_RUNTIME_ROW_ID;
    MaterialRuntimeRowID other_build  = INVALID_MATERIAL_RUNTIME_ROW_ID;
    MaterialRuntimeRowID other_data   = INVALID_MATERIAL_RUNTIME_ROW_ID;
    {
        other_build  = table.Intern(MaterialRuntimeKey{0x9999ull, 0x2222ull, 7u});  // 同配方/数据行，不同 program 身份
        other_recipe = table.Intern(MaterialRuntimeKey{0x1111ull, 0x3333ull, 7u});  // 同 program/数据行，不同配方
        other_data   = table.Intern(MaterialRuntimeKey{0x1111ull, 0x2222ull, 8u});  // 同 program/配方，不同数据行

        if (other_build  == INVALID_MATERIAL_RUNTIME_ROW_ID
         || other_recipe == INVALID_MATERIAL_RUNTIME_ROW_ID
         || other_data   == INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            GLogError(u8"Test 2 Failed: 正常登记不该返回 INVALID");
            return 11;
        }

        if (other_build == shared_row || other_recipe == shared_row || other_data == shared_row
         || other_build == other_recipe || other_build == other_data || other_recipe == other_data)
        {
            GLogError(u8"Test 2 Failed: 键任一维不同必须分裂成不同行"
                      u8"（shared=%u build=%u recipe=%u data=%u）",
                      shared_row, other_build, other_recipe, other_data);
            return 11;
        }

        if (table.GetCount() != 4 || table.GetSharedRowCount() != 4)
        {
            GLogError(u8"Test 2 Failed: 四个不同键后应有 4 条共享行（活行 %u 共享 %u）",
                      table.GetCount(), table.GetSharedRowCount());
            return 11;
        }

        if (table.Get(other_data)->key.data_index_row != 8u)
        {
            GLogError(u8"Test 2 Failed: 数据行维度未原样保存");
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 键三维对等参与身份（build=%u recipe=%u data=%u，均与 %u 分裂）。",
                 other_build, other_recipe, other_data, shared_row);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: refcount 归零回收 + 行号复用世代 `+2`（保持奇数）
    // ─────────────────────────────────────────────────────────────
    {
        const MaterialRuntimeKey key{0x1111ull, 0x2222ull, 7u};

        // 当前 refcount = 2（Test 1 两次 Intern）；再各释放一次即归零
        const uint32_t generation_before = table.GetGeneration(shared_row);

        if (table.Release(shared_row) != false)
        {
            GLogError(u8"Test 3 Failed: refcount 2→1 不该回收");
            return 12;
        }

        if (table.GetRefCount(shared_row) != 1)
        {
            GLogError(u8"Test 3 Failed: 释放一次后 refcount 应为 1，实为 %u",
                      table.GetRefCount(shared_row));
            return 12;
        }

        if (table.Release(shared_row) != true)
        {
            GLogError(u8"Test 3 Failed: refcount 1→0 必须回收");
            return 12;
        }

        if (table.Get(shared_row) != nullptr || table.GetGeneration(shared_row) != 0)
        {
            GLogError(u8"Test 3 Failed: 回收后行应不可取且世代返回 0（死行约定）");
            return 12;
        }

        if (table.GetCount() != 3 || table.GetFreeRowCount() != 1)
        {
            GLogError(u8"Test 3 Failed: 回收后活行应为 3、空闲行 1（活行 %u 空闲 %u）",
                      table.GetCount(), table.GetFreeRowCount());
            return 12;
        }

        // 复用：同键再 Intern 会拿回被回收的行号（free list 就一个）
        const MaterialRuntimeRowID reused = table.Intern(key);

        if (reused != shared_row)
        {
            GLogError(u8"Test 3 Failed: 行号复用未命中刚回收的行（%u != %u）",
                      reused, shared_row);
            return 12;
        }

        const uint32_t generation_after = table.GetGeneration(reused);

        if (generation_after != generation_before + 2 || (generation_after % 2) == 0)
        {
            GLogError(u8"Test 3 Failed: 复用世代必须 +2 保持奇数（%u → %u）",
                      generation_before, generation_after);
            return 12;
        }

        if (table.GetRefCount(reused) != 1 || table.Get(reused)->owned)
        {
            GLogError(u8"Test 3 Failed: 复用行应是全新的共享行（refcount=1、非自有）");
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: refcount 归零回收 + 行号复用世代 %u → %u（保持奇数）。",
                 generation_before, generation_after);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: 写时分裂（CoW）—— 多处引用时分裂出自有行；独占时就地转自有；幂等
    // ─────────────────────────────────────────────────────────────
    {
        const MaterialRuntimeKey key{0xAAAAull, 0xBBBBull, 3u};

        const MaterialRuntimeRowID shared = table.Intern(key);
        table.AddRef(shared);                       // 第二个实例引用同一行

        MaterialRuntimeRow *row = table.GetMutable(shared);
        row->recipe_hash = 0xC0FFEEull;
        row->cached_effective_recipe_hash = 0xBEEFull;
        row->material_texture_configuration_hash = 0x1234ull;

        if (table.GetRefCount(shared) != 2)
        {
            GLogError(u8"Test 4 Failed: 前置条件（refcount=2）不成立，实为 %u",
                      table.GetRefCount(shared));
            return 13;
        }

        const uint32_t shared_count_before = table.GetSharedRowCount();
        const uint32_t owned_count_before  = table.GetOwnedRowCount();

        const MaterialRuntimeRowID owned = table.DetachOwned(shared);

        if (owned == INVALID_MATERIAL_RUNTIME_ROW_ID || owned == shared)
        {
            GLogError(u8"Test 4 Failed: 多处引用的共享行 DetachOwned 应分裂出新行（shared=%u owned=%u）",
                      shared, owned);
            return 13;
        }

        if (table.GetRefCount(shared) != 1)
        {
            GLogError(u8"Test 4 Failed: 分裂后源行 refcount 应 -1（=1），实为 %u",
                      table.GetRefCount(shared));
            return 13;
        }

        const MaterialRuntimeRow *owned_row = table.Get(owned);
        if (!owned_row || !owned_row->owned || owned_row->refcount != 1
         || owned_row->recipe_hash != 0xC0FFEEull
         || owned_row->cached_effective_recipe_hash != 0xBEEFull
         || owned_row->material_texture_configuration_hash != 0x1234ull)
        {
            GLogError(u8"Test 4 Failed: 自有行未按源行内容拷贝（或不是自有/refcount!=1）");
            return 13;
        }

        if (table.GetSharedRowCount() != shared_count_before
         || table.GetOwnedRowCount()  != owned_count_before + 1)
        {
            GLogError(u8"Test 4 Failed: 共享/自有行计数未按分裂移动（共享 %u→%u 自有 %u→%u）",
                      shared_count_before, table.GetSharedRowCount(),
                      owned_count_before, table.GetOwnedRowCount());
            return 13;
        }

        // 自有行不入键索引：同键再 Intern 命中**源行**，不是自有行
        if (table.Intern(key) != shared)
        {
            GLogError(u8"Test 4 Failed: 自有行混进了键索引（同键 Intern 命中了自有行）");
            return 13;
        }
        table.Release(shared);      // 归还上面那次 Intern

        // 已是自有行 ⇒ 幂等
        if (table.DetachOwned(owned) != owned)
        {
            GLogError(u8"Test 4 Failed: 已是自有行的 DetachOwned 必须幂等");
            return 13;
        }

        // 独占的共享行 ⇒ 就地转自有（不新增行）
        const MaterialRuntimeRowID solo = table.Intern(MaterialRuntimeKey{0xDDDDull, 0xEEEEull, 9u});
        const uint32_t count_before_solo = table.GetCount();
        const MaterialRuntimeRowID solo_owned = table.DetachOwned(solo);

        if (solo_owned != solo || table.GetCount() != count_before_solo
         || !table.Get(solo)->owned || table.Get(solo)->refcount != 1)
        {
            GLogError(u8"Test 4 Failed: 独占共享行应**就地**转自有（solo=%u owned=%u count=%u→%u）",
                      solo, solo_owned, count_before_solo, table.GetCount());
            return 13;
        }

        // 就地转自有的行也不再被同键命中 ⇒ 新 Intern 得到另一条共享行
        const MaterialRuntimeRowID solo_new = table.Intern(MaterialRuntimeKey{0xDDDDull, 0xEEEEull, 9u});
        if (solo_new == solo)
        {
            GLogError(u8"Test 4 Failed: 已转自有的行仍在键索引里");
            return 13;
        }

        GLogInfo(u8"Test 4 Passed: CoW（多处引用→分裂%u；独占→就地转自有%u；幂等）。",
                 owned, solo_owned);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 5: 每实例 slot —— 稀疏存放、幂等取/建、每实例状态独立、销毁释放行引用
    // ─────────────────────────────────────────────────────────────
    {
        const EntityID entity_a(1, 1);
        const EntityID entity_b(2, 1);

        MaterialRuntimeSlot &slot_a = table.GetOrCreateSlot(entity_a);
        MaterialRuntimeSlot &slot_b = table.GetOrCreateSlot(entity_b);

        if (&table.GetOrCreateSlot(entity_a) != &slot_a)
        {
            GLogError(u8"Test 5 Failed: 同实体两次取 slot 不是同一个（幂等失效）");
            return 14;
        }

        if (&slot_a == &slot_b || table.GetSlotCount() != 2)
        {
            GLogError(u8"Test 5 Failed: 不同实体必须各持一个 slot（count=%u）",
                      table.GetSlotCount());
            return 14;
        }

        if (table.GetSlot(EntityID(3, 1)) != nullptr)
        {
            GLogError(u8"Test 5 Failed: 未建 slot 的实体必须返回空");
            return 14;
        }

        // 两个实例引用**同一行**（同授权态）
        const MaterialRuntimeKey shared_key{0x5555ull, 0x6666ull, 0u};
        const MaterialRuntimeRowID row_id = table.Intern(shared_key);
        slot_a.row = row_id;
        slot_b.row = table.Intern(shared_key);      // 同键 ⇒ 同一行（refcount 2）

        if (slot_a.row != slot_b.row || table.GetRefCount(row_id) != 2)
        {
            GLogError(u8"Test 5 Failed: 同授权态的两个实例未引用同一行（%u / %u refcount=%u）",
                      slot_a.row, slot_b.row, table.GetRefCount(row_id));
            return 14;
        }

        // 每实例状态独立：改 A 的选择器/重试计数/脏标志，B 不受影响
        slot_a.pass_selector = 1;
        slot_a.lod_selector = 2;
        slot_a.dither_enabled = true;
        slot_a.shadow_retry_frames = 77;
        slot_a.program_dirty = false;
        slot_a.runtime_dirty = false;
        slot_a.valid = true;

        if (slot_b.pass_selector != 0 || slot_b.lod_selector != 0
         || slot_b.dither_enabled || slot_b.shadow_retry_frames != 0
         || !slot_b.program_dirty || !slot_b.runtime_dirty || slot_b.valid)
        {
            GLogError(u8"Test 5 Failed: 每实例状态互相污染（每实例状态必须只在自己 slot 里）");
            return 14;
        }

        // 每实例状态**不进共享行、也不进行键**：改完再 Intern 同键仍是同一行
        if (table.Intern(shared_key) != row_id || table.GetCount() != table.GetSharedRowCount() + table.GetOwnedRowCount())
        {
            GLogError(u8"Test 5 Failed: 每实例状态疑似影响了行身份（同键 Intern 未命中同一行）");
            return 14;
        }
        table.Release(row_id);

        // 销毁 slot ⇒ 释放它持有的行引用
        const uint32_t ref_before = table.GetRefCount(row_id);
        if (!table.DestroySlot(entity_b) || table.GetSlot(entity_b) != nullptr)
        {
            GLogError(u8"Test 5 Failed: DestroySlot 未生效");
            return 14;
        }

        if (table.GetRefCount(row_id) != ref_before - 1 || table.GetSlotCount() != 1)
        {
            GLogError(u8"Test 5 Failed: 销毁 slot 未释放行引用（%u → %u，slot 数 %u）",
                      ref_before, table.GetRefCount(row_id), table.GetSlotCount());
            return 14;
        }

        if (table.DestroySlot(entity_b))
        {
            GLogError(u8"Test 5 Failed: 重复销毁同一 slot 应返回 false");
            return 14;
        }

        GLogInfo(u8"Test 5 Passed: slot 幂等取/建、每实例状态独立、销毁释放行引用（refcount %u → %u）。",
                 ref_before, table.GetRefCount(row_id));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 6: 预算上限（超限 ⇒ INVALID + 计数不变 + 只告警一次）
    // ─────────────────────────────────────────────────────────────
    {
        table.Clear();

        if (table.GetCount() != 0 || table.GetSlotCount() != 0
         || table.GetOverflowWarnCount() != 0 || table.GetFreeRowCount() != 0)
        {
            GLogError(u8"Test 6 Failed: Clear 后计数/告警计数未归零（活行 %u slot %u warn %u 空闲 %u）",
                      table.GetCount(), table.GetSlotCount(),
                      table.GetOverflowWarnCount(), table.GetFreeRowCount());
            return 15;
        }

        const uint32_t limit = MaterialRuntimeTable::kMaterialRuntimeCapacityLimit;

        for (uint32_t i = 0; i < limit; ++i)
        {
            if (table.Intern(MaterialRuntimeKey{static_cast<uint64_t>(i) + 1, 0, i}) == INVALID_MATERIAL_RUNTIME_ROW_ID)
            {
                GLogError(u8"Test 6 Failed: 未达上限（第 %u 个）就拒绝登记", i + 1);
                return 15;
            }
        }

        if (table.GetCount() != limit)
        {
            GLogError(u8"Test 6 Failed: 满额登记后活行应为 %u，实为 %u", limit, table.GetCount());
            return 15;
        }

        const MaterialRuntimeRowID over1 = table.Intern(MaterialRuntimeKey{static_cast<uint64_t>(limit) + 1, 0, 0});
        const MaterialRuntimeRowID over2 = table.Intern(MaterialRuntimeKey{static_cast<uint64_t>(limit) + 2, 0, 0});

        if (over1 != INVALID_MATERIAL_RUNTIME_ROW_ID || over2 != INVALID_MATERIAL_RUNTIME_ROW_ID)
        {
            GLogError(u8"Test 6 Failed: 超限新键必须返回 INVALID（over1=%u over2=%u）", over1, over2);
            return 15;
        }

        if (table.GetCount() != limit)
        {
            GLogError(u8"Test 6 Failed: 超限被静默扩容（活行 %u != %u）", table.GetCount(), limit);
            return 15;
        }

        if (table.GetOverflowWarnCount() != 1)
        {
            GLogError(u8"Test 6 Failed: 超限告警次数应为 1（只告警一次），实为 %u",
                      table.GetOverflowWarnCount());
            return 15;
        }

        // 已登记键不受上限影响：仍命中原行（`Intern` 会 +refcount，随即归还）
        {
            const MaterialRuntimeRowID hit = table.Intern(MaterialRuntimeKey{1, 0, 0});
            if (hit == INVALID_MATERIAL_RUNTIME_ROW_ID || table.GetCount() != limit)
            {
                GLogError(u8"Test 6 Failed: 满额后已登记键不再命中");
                return 15;
            }
            table.Release(hit);
        }

        GLogInfo(u8"Test 6 Passed: 预算上限 %u 生效——超限返回 INVALID、计数不变、只告警一次。", limit);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 7: Clear 复位（行 / 索引 / slot / 告警闩都归零，行号与世代从头开始）
    // ─────────────────────────────────────────────────────────────
    {
        table.Clear();
        table.GetOrCreateSlot(EntityID(9, 1));

        if (table.GetCount() != 0 || table.GetSlotCount() != 1 || table.GetOverflowWarnCount() != 0)
        {
            GLogError(u8"Test 7 Failed: Clear 后建 slot 的前置计数异常（活行 %u slot %u warn %u）",
                      table.GetCount(), table.GetSlotCount(), table.GetOverflowWarnCount());
            return 16;
        }

        table.Clear();

        if (table.GetCount() != 0 || table.GetSlotCount() != 0
         || table.GetFreeRowCount() != 0 || table.GetOverflowWarnCount() != 0)
        {
            GLogError(u8"Test 7 Failed: Clear 未复位（活行 %u slot %u 空闲 %u warn %u）",
                      table.GetCount(), table.GetSlotCount(),
                      table.GetFreeRowCount(), table.GetOverflowWarnCount());
            return 16;
        }

        if (table.Intern(MaterialRuntimeKey{42, 43, 44}) != 0
         || table.GetGeneration(0) != 1)
        {
            GLogError(u8"Test 7 Failed: Clear 后首个登记应为行号 0、世代 1");
            return 16;
        }

        table.Clear();

        GLogInfo(u8"Test 7 Passed: Clear 复位行/索引/slot/告警闩（行号与世代从头开始）。");
    }

    GLogInfo(u8"=== All MaterialRuntimeTable tests passed successfully! ===");
    return 0;
}
