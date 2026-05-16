// BindingAllocator unit tests — standalone executable, no external test framework.
//
// Covers:
//   Auto_AssignsSequentialBindings
//   FixedSet_RespectsSet_AutoBinding
//   FixedSetAndBinding_OccupiesExactSlot
//   Conflict_TwoFixedSetAndBinding_SameSlot_ReportsError
//   Conflict_AutoOverlapsFixed_ReportsError   (Auto skips occupied slots — ok=true)
//   StableOrderByDebugName_AcrossRuns
//
// Failure: CHECK_* prints a message and increments g_failures.
// Success: main() returns 0.

#include "../ColorSource/BindingAllocator.h"

#include <cstdio>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Harness
// ---------------------------------------------------------------------------
static int g_failures = 0;

#define CHECK_TRUE(expr)                                                        \
    do {                                                                        \
        if (!(expr)) {                                                          \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                        \
                         __FILE__, __LINE__, #expr);                            \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))
#define CHECK_NE(a, b)  CHECK_TRUE((a) != (b))

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
using namespace hgl::graph;

static DescriptorRequirement MakeAuto(const std::string &name)
{
    DescriptorRequirement r;
    r.binding_policy = BindingPolicy::Auto;
    r.debug_name     = name;
    return r;
}

static DescriptorRequirement MakeFixedSet(const std::string &name, uint32_t set)
{
    DescriptorRequirement r;
    r.binding_policy = BindingPolicy::FixedSet;
    r.fixed_set      = set;
    r.debug_name     = name;
    return r;
}

static DescriptorRequirement MakeFixedSetAndBinding(const std::string &name,
                                                    uint32_t set, uint32_t binding)
{
    DescriptorRequirement r;
    r.binding_policy  = BindingPolicy::FixedSetAndBinding;
    r.fixed_set       = set;
    r.fixed_binding   = binding;
    r.debug_name      = name;
    return r;
}

// Returns the resolved binding for debug_name, or {~0u, ~0u} if not found.
static ResolvedBinding Find(const BindingAllocResult &res, const std::string &name)
{
    for (const auto &rb : res.bindings)
        if (rb.debug_name == name)
            return rb;
    return { ~0u, ~0u, name };
}

// ---------------------------------------------------------------------------
// Test 1 — Auto_AssignsSequentialBindings
// ---------------------------------------------------------------------------
static void Auto_AssignsSequentialBindings()
{
    BindingAllocator alloc;
    alloc.AddRequirements({ MakeAuto("B"), MakeAuto("A"), MakeAuto("C") });
    const auto res = alloc.Allocate();

    CHECK_TRUE(res.ok);
    CHECK_EQ(res.bindings.size(), size_t(3));

    // Sorted by debug_name: A=0, B=1, C=2, all in kDefaultMaterialBindingSet=3
    const auto a = Find(res, "A");
    const auto b = Find(res, "B");
    const auto c = Find(res, "C");

    CHECK_EQ(a.set, kDefaultMaterialBindingSet);
    CHECK_EQ(b.set, kDefaultMaterialBindingSet);
    CHECK_EQ(c.set, kDefaultMaterialBindingSet);

    CHECK_EQ(a.binding, 0u);
    CHECK_EQ(b.binding, 1u);
    CHECK_EQ(c.binding, 2u);
}

// ---------------------------------------------------------------------------
// Test 2 — FixedSet_RespectsSet_AutoBinding
// ---------------------------------------------------------------------------
static void FixedSet_RespectsSet_AutoBinding()
{
    BindingAllocator alloc;
    // Two FixedSet entries in set=2, one Auto entry
    alloc.AddRequirements({
        MakeFixedSet("FS_B", 2),
        MakeFixedSet("FS_A", 2),
        MakeAuto("AUTO"),
    });
    const auto res = alloc.Allocate();

    CHECK_TRUE(res.ok);

    const auto a = Find(res, "FS_A");
    const auto b = Find(res, "FS_B");
    const auto au = Find(res, "AUTO");

    // FixedSet entries land in set=2
    CHECK_EQ(a.set, 2u);
    CHECK_EQ(b.set, 2u);

    // Sorted dict order: FS_A=0, FS_B=1
    CHECK_EQ(a.binding, 0u);
    CHECK_EQ(b.binding, 1u);

    // Auto goes to kDefaultMaterialBindingSet=3
    CHECK_EQ(au.set, kDefaultMaterialBindingSet);
    CHECK_EQ(au.binding, 0u);
}

// ---------------------------------------------------------------------------
// Test 3 — FixedSetAndBinding_OccupiesExactSlot
// ---------------------------------------------------------------------------
static void FixedSetAndBinding_OccupiesExactSlot()
{
    BindingAllocator alloc;
    alloc.AddRequirements({ MakeFixedSetAndBinding("TX", 1, 5) });
    const auto res = alloc.Allocate();

    CHECK_TRUE(res.ok);
    CHECK_TRUE(res.diags.empty());

    const auto tx = Find(res, "TX");
    CHECK_EQ(tx.set,     1u);
    CHECK_EQ(tx.binding, 5u);
}

// ---------------------------------------------------------------------------
// Test 4 — Conflict_TwoFixedSetAndBinding_SameSlot_ReportsError
// ---------------------------------------------------------------------------
static void Conflict_TwoFixedSetAndBinding_SameSlot_ReportsError()
{
    BindingAllocator alloc;
    alloc.AddRequirements({
        MakeFixedSetAndBinding("X", 0, 3),
        MakeFixedSetAndBinding("Y", 0, 3),  // same (set=0, binding=3)
    });
    const auto res = alloc.Allocate();

    CHECK_TRUE(!res.ok);

    bool found_error = false;
    for (const auto &d : res.diags)
        if (d.level == BindingAllocDiag::Level::Error)
            found_error = true;
    CHECK_TRUE(found_error);
}

// ---------------------------------------------------------------------------
// Test 5 — Conflict_AutoOverlapsFixed_ReportsError
//   Auto allocation must skip FixedSetAndBinding-occupied binding numbers,
//   so the result is still ok=true and the Auto binding is assigned elsewhere.
// ---------------------------------------------------------------------------
static void Conflict_AutoOverlapsFixed_ReportsError()
{
    // FixedSetAndBinding takes (set=3, binding=0) in kDefaultMaterialBindingSet.
    // An Auto entry would normally get binding=0 in set=3 — it should be bumped
    // to binding=1 instead, and the allocation must succeed.
    BindingAllocator alloc;
    alloc.AddRequirements({
        MakeFixedSetAndBinding("FIXED", kDefaultMaterialBindingSet, 0),
        MakeAuto("AUTO"),
    });
    const auto res = alloc.Allocate();

    CHECK_TRUE(res.ok);  // no actual conflict — Auto skips occupied slot

    const auto fixed = Find(res, "FIXED");
    const auto au    = Find(res, "AUTO");

    CHECK_EQ(fixed.set,     kDefaultMaterialBindingSet);
    CHECK_EQ(fixed.binding, 0u);

    CHECK_EQ(au.set,     kDefaultMaterialBindingSet);
    CHECK_EQ(au.binding, 1u);  // bumped past the fixed slot
}

// ---------------------------------------------------------------------------
// Test 6 — StableOrderByDebugName_AcrossRuns
//   Adding entries in different orders must produce identical bindings.
// ---------------------------------------------------------------------------
static void StableOrderByDebugName_AcrossRuns()
{
    auto run = [](bool reverse) -> BindingAllocResult {
        BindingAllocator alloc;
        std::vector<DescriptorRequirement> reqs = {
            MakeAuto("Sampler_Emissive"),
            MakeAuto("Sampler_Normal"),
            MakeAuto("Sampler_BaseColor"),
        };
        if (reverse)
            std::reverse(reqs.begin(), reqs.end());
        alloc.AddRequirements(reqs);
        return alloc.Allocate();
    };

    const auto r1 = run(false);
    const auto r2 = run(true);

    CHECK_TRUE(r1.ok);
    CHECK_TRUE(r2.ok);

    // Each name should resolve to the same (set, binding) in both runs.
    for (const std::string &name : { std::string("Sampler_BaseColor"),
                                     std::string("Sampler_Emissive"),
                                     std::string("Sampler_Normal") })
    {
        const auto b1 = Find(r1, name);
        const auto b2 = Find(r2, name);
        CHECK_EQ(b1.set,     b2.set);
        CHECK_EQ(b1.binding, b2.binding);
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    Auto_AssignsSequentialBindings();
    FixedSet_RespectsSet_AutoBinding();
    FixedSetAndBinding_OccupiesExactSlot();
    Conflict_TwoFixedSetAndBinding_SameSlot_ReportsError();
    Conflict_AutoOverlapsFixed_ReportsError();
    StableOrderByDebugName_AcrossRuns();

    if (g_failures == 0)
    {
        std::fprintf(stdout, "BindingAllocatorTests: all tests passed.\n");
        return 0;
    }
    std::fprintf(stderr, "BindingAllocatorTests: %d test(s) FAILED.\n", g_failures);
    return 1;
}
