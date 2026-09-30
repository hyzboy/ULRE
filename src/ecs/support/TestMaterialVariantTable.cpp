#include <hgl/ecs/support/MaterialVariantTable.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    /// 造一个非空 program 占位指针（只验证"记录可读写"，不解引用）。
    hgl::graph::ShaderProgram *FakeProgram(const uintptr_t v)
    {
        return reinterpret_cast<hgl::graph::ShaderProgram *>(v);
    }
}

/**
 * A3 材质变体表（v2 §9.3）
 *
 *   · 表是**按静态键去重**的：同键两次 Intern ⇒ 同 ID 且计数不增；
 *   · 键是**两维**的（build context + recipe 身份）：同 build context 下**不同材质**
 *     必须得到不同 ID / 两条记录——否则后解析者的 program 覆盖先解析者（键粒度回归钉）；
 *   · 记录可读写（program 归属记录：同键共享、异键互不影响）——A4 起**不再**在记录上
 *     承载重试计数/生命周期位等每实例状态（已迁 `MaterialRuntimeSlot`）；
 *   · 预算制：超容量上限 fail-fast（返回 INVALID）且**只告警一次**，不静默扩容。
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestMaterialVariantTable"));

    GLogInfo(u8"=== Testing 材质变体表（A3）===");

    MaterialVariantTable table;

    // ─────────────────────────────────────────────────────────────
    // Test 1: 同键去重（两次 Intern 同键 ⇒ 同 ID、GetCount() 不增）
    // ─────────────────────────────────────────────────────────────
    {
        const MaterialVariantID a = table.Intern(MaterialVariantKey{111});
        const uint32_t count_after_first = table.GetCount();
        const MaterialVariantID b = table.Intern(MaterialVariantKey{111});

        if (a == INVALID_MATERIAL_VARIANT_ID || b == INVALID_MATERIAL_VARIANT_ID)
        {
            GLogError(u8"Test 1 Failed: 正常登记不该返回 INVALID（a=%u b=%u）", a, b);
            return 10;
        }

        if (a != b)
        {
            GLogError(u8"Test 1 Failed: 同键两次 Intern 返回了不同 ID（%u != %u）——去重失效", a, b);
            return 10;
        }

        if (table.GetCount() != count_after_first || count_after_first != 1)
        {
            GLogError(u8"Test 1 Failed: 同键重复 Intern 让计数增长（%u → %u）",
                      count_after_first, table.GetCount());
            return 10;
        }

        if (!table.Get(a) || table.Get(a)->key.program_build_context != 111)
        {
            GLogError(u8"Test 1 Failed: 记录缺失或键不是登记时的 111");
            return 10;
        }

        GLogInfo(u8"Test 1 Passed: 同键去重（ID=%u，计数保持 %u）。", a, table.GetCount());
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 不同键 ⇒ 不同 ID
    // ─────────────────────────────────────────────────────────────
    const MaterialVariantID id_111 = table.Intern(MaterialVariantKey{111});
    MaterialVariantID id_222 = INVALID_MATERIAL_VARIANT_ID;
    {
        id_222 = table.Intern(MaterialVariantKey{222});

        if (id_222 == INVALID_MATERIAL_VARIANT_ID || id_222 == id_111)
        {
            GLogError(u8"Test 2 Failed: 不同键必须得到不同 ID（111→%u，222→%u）", id_111, id_222);
            return 11;
        }

        if (table.GetCount() != 2)
        {
            GLogError(u8"Test 2 Failed: 两个不同键后计数应为 2，实为 %u", table.GetCount());
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 不同键 ⇒ 不同 ID（%u / %u）。", id_111, id_222);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: 记录可读写（program；A4 起记录只承载 program 身份这一项可共享状态）
    //   注意：记录是**同键共享**的，写入必须能经 Get 读回。
    // ─────────────────────────────────────────────────────────────
    {
        MaterialVariantRecord *mut = table.GetMutable(id_111);
        if (!mut)
        {
            GLogError(u8"Test 3 Failed: GetMutable(有效 ID) 返回空");
            return 12;
        }

        mut->program = FakeProgram(0x1234);

        const MaterialVariantRecord *ro = table.Get(id_111);
        if (!ro
         || ro->program != FakeProgram(0x1234))
        {
            GLogError(u8"Test 3 Failed: 记录写入未按原值读回");
            return 12;
        }

        if (table.GetMutable(INVALID_MATERIAL_VARIANT_ID) != nullptr
         || table.Get(INVALID_MATERIAL_VARIANT_ID) != nullptr
         || table.Get(99999u) != nullptr)
        {
            GLogError(u8"Test 3 Failed: 非法 ID 必须返回空（不能越界取到记录）");
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: 记录可读写，非法 ID 返回空。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: program 归属记录（异键互不影响；同键共享同一份）
    // ─────────────────────────────────────────────────────────────
    {
        MaterialVariantRecord *other = table.GetMutable(id_222);
        if (!other)
        {
            GLogError(u8"Test 4 Failed: GetMutable(第二键) 返回空");
            return 13;
        }

        other->program = FakeProgram(0x2222);

        if (other->program != FakeProgram(0x2222))
        {
            GLogError(u8"Test 4 Failed: 记录自身的 program 读回异常");
            return 13;
        }

        if (table.Get(id_111)->program != FakeProgram(0x1234))
        {
            GLogError(u8"Test 4 Failed: 异键 program 互相污染（111 的 program 被改写成别人的）");
            return 13;
        }

        // 同键再 Intern 拿到的仍是同一条记录 ⇒ program 是"该变体"共享的
        const MaterialVariantID again = table.Intern(MaterialVariantKey{111});
        if (again != id_111 || table.Get(again)->program != FakeProgram(0x1234))
        {
            GLogError(u8"Test 4 Failed: 同键再 Intern 未复用同一条记录的 program（id=%u）", again);
            return 13;
        }

        GLogInfo(u8"Test 4 Passed: program 归属记录（111→原值，222→异值，互不影响）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 5: 预算上限（超限 ⇒ INVALID + 计数不变 + 只告警一次）
    //   键都用 i+1（避开 0，避免把"默认值"混进这条用例）。
    // ─────────────────────────────────────────────────────────────
    {
        table.Clear();

        if (table.GetCount() != 0 || table.GetOverflowWarnCount() != 0)
        {
            GLogError(u8"Test 5 Failed: Clear 后计数/告警计数未归零（count=%u warn=%u）",
                      table.GetCount(), table.GetOverflowWarnCount());
            return 14;
        }

        const uint32_t limit = MaterialVariantTable::kMaterialVariantCapacityLimit;

        for (uint32_t i = 0; i < limit; ++i)
        {
            if (table.Intern(MaterialVariantKey{static_cast<uint64_t>(i) + 1}) == INVALID_MATERIAL_VARIANT_ID)
            {
                GLogError(u8"Test 5 Failed: 未达上限（第 %u 个）就拒绝登记", i + 1);
                return 14;
            }
        }

        if (table.GetCount() != limit)
        {
            GLogError(u8"Test 5 Failed: 满额登记后计数应为 %u，实为 %u", limit, table.GetCount());
            return 14;
        }

        // 超限：两个不同的新键都必须 fail-fast，且计数不增
        const MaterialVariantID over1 = table.Intern(MaterialVariantKey{static_cast<uint64_t>(limit) + 1});
        const MaterialVariantID over2 = table.Intern(MaterialVariantKey{static_cast<uint64_t>(limit) + 2});

        if (over1 != INVALID_MATERIAL_VARIANT_ID || over2 != INVALID_MATERIAL_VARIANT_ID)
        {
            GLogError(u8"Test 5 Failed: 超限新键必须返回 INVALID（over1=%u over2=%u）", over1, over2);
            return 14;
        }

        if (table.GetCount() != limit)
        {
            GLogError(u8"Test 5 Failed: 超限被静默扩容（计数 %u != %u）", table.GetCount(), limit);
            return 14;
        }

        if (table.GetOverflowWarnCount() != 1)
        {
            GLogError(u8"Test 5 Failed: 超限告警次数应为 1（只告警一次），实为 %u",
                      table.GetOverflowWarnCount());
            return 14;
        }

        // 已登记键不受上限影响：仍命中原 ID
        if (table.Intern(MaterialVariantKey{1}) != 0)
        {
            GLogError(u8"Test 5 Failed: 满额后已登记键不再命中（应为 ID 0）");
            return 14;
        }

        GLogInfo(u8"Test 5 Passed: 预算上限 %u 生效——超限返回 INVALID、计数不变、只告警一次。", limit);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 6: Clear 复位（记录 / 索引 / 告警闩都归零，ID 从头开始）
    // ─────────────────────────────────────────────────────────────
    {
        table.Clear();

        if (table.GetCount() != 0 || table.GetOverflowWarnCount() != 0)
        {
            GLogError(u8"Test 6 Failed: Clear 未复位（count=%u warn=%u）",
                      table.GetCount(), table.GetOverflowWarnCount());
            return 15;
        }

        if (table.Intern(MaterialVariantKey{42}) != 0)
        {
            GLogError(u8"Test 6 Failed: Clear 后首个登记应为 ID 0");
            return 15;
        }

        GLogInfo(u8"Test 6 Passed: Clear 复位记录/索引/告警闩。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 7: **同 build context、不同 recipe 身份 ⇒ 两个 ID、两条记录**
    //
    //   A3 键粒度 bug 的回归钉：键只有 build_context 时，两种材质（不同 recipe、同
    //   primitive_type/顶点格式/profile/purpose）会落到**同一条**变体记录，program
    //   被后写覆盖——先解析者拿到别人的 program（4 个窗口示例每例只有一种 recipe，
    //   所以没暴露）。这里用两个稳定的 recipe 指纹模拟两种材质（不需要真建材质），
    //   并证明两维中任一维不同都必然分裂。
    // ─────────────────────────────────────────────────────────────
    {
        table.Clear();

        const uint64_t kBuildContext = 0x5A5A1234567890ABull;   // 同图元/顶点格式/profile/purpose
        const uint64_t recipe_a     = 0x1111222233334444ull;    // 材质 A 的配方指纹
        const uint64_t recipe_b     = 0x9999AAAABBBBCCCCull;    // 材质 B 的配方指纹

        const MaterialVariantID variant_a =
            table.Intern(MaterialVariantKey{kBuildContext, recipe_a});
        const uint32_t count_after_a = table.GetCount();
        const MaterialVariantID variant_b =
            table.Intern(MaterialVariantKey{kBuildContext, recipe_b});

        if (variant_a == INVALID_MATERIAL_VARIANT_ID
         || variant_b == INVALID_MATERIAL_VARIANT_ID)
        {
            GLogError(u8"Test 7 Failed: 正常登记不该返回 INVALID（a=%u b=%u）",
                      variant_a, variant_b);
            return 16;
        }

        if (variant_a == variant_b)
        {
            GLogError(u8"Test 7 Failed: 同 build context、不同 recipe 身份落到了同一条变体"
                      u8"记录（ID=%u）——不同材质的 program 会互相覆盖",
                      variant_a);
            return 16;
        }

        if (count_after_a != 1 || table.GetCount() != 2)
        {
            GLogError(u8"Test 7 Failed: 不同 recipe 必须各占一条记录（a 后 %u，b 后 %u）",
                      count_after_a, table.GetCount());
            return 16;
        }

        // 两条记录各自保留完整键（program 归属可由记录反查；键残缺 ⇒ 反查会错）
        const MaterialVariantRecord *rec_a = table.Get(variant_a);
        const MaterialVariantRecord *rec_b = table.Get(variant_b);

        if (!rec_a || !rec_b
         || rec_a->key.program_build_context != kBuildContext
         || rec_b->key.program_build_context != kBuildContext
         || rec_a->key.recipe_hash != recipe_a
         || rec_b->key.recipe_hash != recipe_b)
        {
            GLogError(u8"Test 7 Failed: 两条记录的键不是登记时的值（键残缺）");
            return 16;
        }

        // 记录互相独立：各写各的 program，不得串味（program 被覆盖正是原 bug 的形态）
        table.GetMutable(variant_a)->program = FakeProgram(0xA1);
        table.GetMutable(variant_b)->program = FakeProgram(0xB2);

        rec_a = table.Get(variant_a);
        rec_b = table.Get(variant_b);
        if (!rec_a || !rec_b
         || rec_a->program != FakeProgram(0xA1)
         || rec_b->program != FakeProgram(0xB2))
        {
            GLogError(u8"Test 7 Failed: 两条记录的 program 互相污染（A 的 program 被写成 B 的）");
            return 16;
        }

        // 同 (build context, recipe) 再 Intern ⇒ 命中原 ID、不增记录（去重仍生效）
        if (table.Intern(MaterialVariantKey{kBuildContext, recipe_a}) != variant_a
         || table.Intern(MaterialVariantKey{kBuildContext, recipe_b}) != variant_b
         || table.GetCount() != 2)
        {
            GLogError(u8"Test 7 Failed: 同 (build context, recipe) 未命中原 ID / 计数被撑大（%u）",
                      table.GetCount());
            return 16;
        }

        // 反向：同一 recipe、不同 build context 也必须分裂（两维对等参与身份）
        const MaterialVariantID variant_c =
            table.Intern(MaterialVariantKey{kBuildContext ^ 0x1ull, recipe_a});
        if (variant_c == INVALID_MATERIAL_VARIANT_ID
         || variant_c == variant_a
         || table.GetCount() != 3)
        {
            GLogError(u8"Test 7 Failed: 同 recipe、不同 build context 未分裂（c=%u a=%u count=%u）",
                      variant_c, variant_a, table.GetCount());
            return 16;
        }

        GLogInfo(u8"Test 7 Passed: 同 build context + 不同 recipe ⇒ 两个 ID（%u / %u），"
                 u8"键两维都参与身份，记录互不串味。",
                 variant_a, variant_b);
    }

    GLogInfo(u8"=== All MaterialVariantTable tests passed successfully! ===");
    return 0;
}
