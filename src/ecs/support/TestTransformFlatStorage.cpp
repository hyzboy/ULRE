#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/support/TransformAccessor.h>
#include <hgl/ecs/support/ComponentTypeTable.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <string>
#include <cstring>
#include <iterator>

using namespace hgl;
using namespace hgl::ecs;

// ─────────────────────────────────────────────────────────────────────────────
// 编译期校验：组件类型表（v2 约束 §1，自动 scale —— 新增类型时这里会立刻报错）
//   · 每类都必须有非空 arena 名（漏登记 ⇒ 编译失败）
//   · 有 GPU 行的类型，行宽必须是 16 的倍数（GPU 最小对齐）
//   · Transform 的 GPU 行宽 = mat4 = 64 B（L2W 派生视图）
// ─────────────────────────────────────────────────────────────────────────────
namespace
{
    constexpr bool CheckComponentTypeTableEntries()
    {
        for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
        {
            const ComponentTypeInfo &info = kComponentTypeTable[i];

            if (info.type == ComponentType::None)      // 占位项：无 arena、无行宽
                continue;

            if (!info.arena_name || info.arena_name[0] == '\0')
                return false;

            if (info.gpu_row_bytes != 0 && (info.gpu_row_bytes % 16) != 0)
                return false;
        }

        return true;
    }
}

static_assert(CheckComponentTypeTableEntries(), "组件类型表项不完整（缺 arena 名，或 GPU 行宽未按 16 字节对齐）");
static_assert(GetComponentGPURowBytes(ComponentType::Transform) == 64, "Transform 的 GPU 行宽应为 mat4 = 64 B");
static_assert(GetComponentScope(ComponentType::Transform) == ComponentScope::World, "Transform 是世界私有数据");

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // 没有这一行时本测试的所有 GLog（含失败原因）都无处可去——静默通过/静默失败
    hgl::logger::InitLogger(OS_TEXT("TestTransformFlatStorage"));

    GLogInfo(u8"=== Testing TransformDataStorage Flat Triplet & Topological Evaluation ===");

    TransformDataStorage storage;

    // Test 1: Allocate flat nodes
    auto root_id = storage.Allocate();
    auto child1_id = storage.Allocate();
    auto child2_id = storage.Allocate();
    auto grand_child_id = storage.Allocate();

    if (storage.GetCount() != 4)
    {
        GLogError(u8"Test 1 Failed: storage.GetCount() != 4");
        return 1;
    }

    // Test 2: Hierarchy setup
    // Root: pos(10, 0, 0)
    storage.SetPosition(root_id, glm::vec3(10.0f, 0.0f, 0.0f));

    // Child1: parent = Root, pos(0, 5, 0)
    storage.SetPosition(child1_id, glm::vec3(0.0f, 5.0f, 0.0f));
    storage.SetParent(child1_id, root_id);

    // Child2: parent = Root, pos(0, -5, 0)
    storage.SetPosition(child2_id, glm::vec3(0.0f, -5.0f, 0.0f));
    storage.SetParent(child2_id, root_id);

    // GrandChild: parent = Child1, pos(0, 0, 2)
    storage.SetPosition(grand_child_id, glm::vec3(0.0f, 0.0f, 2.0f));
    storage.SetParent(grand_child_id, child1_id);

    // Test 3: Topology sort
    storage.RebuildTopologyOrder();

    if (storage.GetLevelCount() != 3)
    {
        GLogError(u8"Test 3 Failed: expected 3 levels, got %u", storage.GetLevelCount());
        return 2;
    }

    if (storage.GetHierarchyDepth(root_id) != 0 ||
        storage.GetHierarchyDepth(child1_id) != 1 ||
        storage.GetHierarchyDepth(child2_id) != 1 ||
        storage.GetHierarchyDepth(grand_child_id) != 2)
    {
        GLogError(u8"Test 3 Failed: depth mismatch");
        return 3;
    }

    // Test 4: Flat matrix evaluation
    storage.UpdateAllWorldMatricesFlat();

    glm::mat4 root_world = storage.GetWorldMatrix(root_id);
    glm::mat4 child1_world = storage.GetWorldMatrix(child1_id);
    glm::mat4 grand_child_world = storage.GetWorldMatrix(grand_child_id);

    glm::vec3 root_pos(root_world[3]);
    glm::vec3 child1_pos(child1_world[3]);
    glm::vec3 grand_child_pos(grand_child_world[3]);

    if (glm::distance(root_pos, glm::vec3(10.0f, 0.0f, 0.0f)) > 1e-4f)
    {
        GLogError(u8"Test 4 Failed: root_pos mismatch");
        return 4;
    }
    if (glm::distance(child1_pos, glm::vec3(10.0f, 5.0f, 0.0f)) > 1e-4f)
    {
        GLogError(u8"Test 4 Failed: child1_pos mismatch");
        return 5;
    }
    if (glm::distance(grand_child_pos, glm::vec3(10.0f, 5.0f, 2.0f)) > 1e-4f)
    {
        GLogError(u8"Test 4 Failed: grand_child_pos mismatch");
        return 6;
    }

    // Test 5: Dynamic root update propagation
    storage.SetPosition(root_id, glm::vec3(20.0f, 0.0f, 0.0f));
    storage.UpdateDirtyWorldMatricesFlat();

    grand_child_world = storage.GetWorldMatrix(grand_child_id);
    grand_child_pos = glm::vec3(grand_child_world[3]);

    if (glm::distance(grand_child_pos, glm::vec3(20.0f, 5.0f, 2.0f)) > 1e-4f)
    {
        GLogError(u8"Test 5 Failed: dynamic cascade update failed, expected (20,5,2), got (%.2f, %.2f, %.2f)",
                 grand_child_pos.x, grand_child_pos.y, grand_child_pos.z);
        return 7;
    }

    // Test 6: Cycle protection
    storage.SetParent(root_id, grand_child_id);
    storage.RebuildTopologyOrder();
    storage.UpdateAllWorldMatricesFlat();
    GLogInfo(u8"Test 6 Passed: Cycle handled successfully without crash");

    // Test 7: ECSContext integration
    ECSContext context;
    auto* ecs_storage = context.GetTransformStorage();
    if (!ecs_storage)
    {
        GLogError(u8"Test 7 Failed: ecs_storage is null");
        return 8;
    }

    auto ent1 = context.CreateEntity<Entity>("RootEnt");
    context.CreateTransform(ent1->GetEntityID(), Mobility::Static);
    auto tc1 = context.GetTransformByEntity(ent1->GetEntityID());
    tc1.SetLocalPosition(glm::vec3(1.0f, 2.0f, 3.0f));

    auto ent2 = context.CreateEntity<Entity>("ChildEnt");
    context.CreateTransform(ent2->GetEntityID(), Mobility::Static);
    auto tc2 = context.GetTransformByEntity(ent2->GetEntityID());
    tc2.SetParent(context.GetTransformID(ent1->GetEntityID()));
    tc2.SetLocalPosition(glm::vec3(0.0f, 10.0f, 0.0f));

    glm::vec3 child_world_pos = tc2.GetWorldPosition();
    if (glm::distance(child_world_pos, glm::vec3(1.0f, 12.0f, 3.0f)) > 1e-4f)
    {
        GLogError(u8"Test 7 Failed: ECS entity hierarchy mismatch, expected (1, 12, 3), got (%.2f, %.2f, %.2f)",
                 child_world_pos.x, child_world_pos.y, child_world_pos.z);
        return 9;
    }

    GLogInfo(u8"Test 7 Passed: ECS entity hierarchy via accessor (world position matches).");

    // ─────────────────────────────────────────────────────────────
    // Test 8: 变换行生命周期契约（T8 把组件里三条"对世界的副作用"搬走，必须仍然成立）
    //   a) SetMobility 必须让世界把该行在 静态/可动 列表之间换边
    //      —— 渲染侧索引映射与上传通道按列表分流；只改存储里的字节 ⇒ 实例取不到行
    //         ⇒ 拿到默认行（单位矩阵）⇒ 全部画在原点、且不动
    //   b) SetParent 必须维护存储的子表（子孙标脏/版本 bump 靠它遍历）
    //   c) 实体销毁必须释放其变换行（否则残行留在列表里，索引映射整体串位）
    // ─────────────────────────────────────────────────────────────
    {
        auto *storage = context.GetTransformStorage();
        if (!storage)
        {
            GLogError(u8"Test 8 Failed: 无变换存储");
            return 10;
        }

        const auto contains = [](const std::vector<TransformID> &list, const TransformID id)
        {
            for (const TransformID t : list)
                if (t == id)
                    return true;
            return false;
        };

        // (a) 以 Static 建行，再迁到 Movable
        auto ent_a = context.CreateEntity<Entity>("MigrateProbe");
        const TransformID id_a = context.CreateTransform(ent_a->GetEntityID(), Mobility::Static);

        if (!contains(context.GetStaticTransforms(), id_a))
        {
            GLogError(u8"Test 8 Failed: 以 Static 建行后未进静态列表");
            return 10;
        }

        context.GetTransform(id_a).SetMobility(Mobility::Movable);

        if (!contains(context.GetMovableTransforms(), id_a))
        {
            GLogError(u8"Test 8 Failed: SetMobility(Movable) 后该行未进可动列表"
                      u8"（渲染侧会取不到它的行 ⇒ 实例画在原点、且永不更新）");
            return 10;
        }

        if (contains(context.GetStaticTransforms(), id_a))
        {
            GLogError(u8"Test 8 Failed: 迁移后旧列表仍留着该行（同一行被两路都算）");
            return 10;
        }

        // (b) SetParent 维护子表 + 解除关系后清理
        auto ent_p = context.CreateEntity<Entity>("ParentProbe");
        auto ent_c = context.CreateEntity<Entity>("ChildProbe");
        const TransformID id_p = context.CreateTransform(ent_p->GetEntityID(), Mobility::Static);
        const TransformID id_c = context.CreateTransform(ent_c->GetEntityID(), Mobility::Static);

        context.GetTransform(id_c).SetParent(id_p);

        if (!storage->HasChild(id_p, id_c))
        {
            GLogError(u8"Test 8 Failed: SetParent 未登记子表（子级不会被标脏/重传）");
            return 10;
        }

        context.GetTransform(id_c).SetParent(INVALID_TRANSFORM_ID);

        if (storage->HasChild(id_p, id_c))
        {
            GLogError(u8"Test 8 Failed: 解除父子关系后子表仍留着子行");
            return 10;
        }

        // (c) 实体销毁 ⇒ 变换行注销（行属于世界，不挂在实体上）
        const EntityID child_entity_id = ent_c->GetEntityID();
        context.DestroyEntity(child_entity_id);

        if (IsValidTransformID(context.GetTransformID(child_entity_id)))
        {
            GLogError(u8"Test 8 Failed: 实体销毁后其变换行未注销（残行会把索引映射串位）");
            return 10;
        }

        // (d) 销毁子行必须在父的子表里也清掉（否则父的子表留残条目，遍历到已释放的行）
        auto ent_p2 = context.CreateEntity<Entity>("ParentProbe2");
        auto ent_c2 = context.CreateEntity<Entity>("ChildProbe2");
        const TransformID id_p2 = context.CreateTransform(ent_p2->GetEntityID(), Mobility::Static);
        const TransformID id_c2 = context.CreateTransform(ent_c2->GetEntityID(), Mobility::Static);

        context.GetTransform(id_c2).SetParent(id_p2);

        if (!storage->HasChild(id_p2, id_c2))
        {
            GLogError(u8"Test 8 Failed: 第二次 SetParent 未登记子表");
            return 10;
        }

        const EntityID child2_entity_id = ent_c2->GetEntityID();
        context.DestroyEntity(child2_entity_id);

        if (storage->HasChild(id_p2, id_c2))
        {
            GLogError(u8"Test 8 Failed: 销毁子行后父的子表仍留残条目");
            return 10;
        }

        GLogInfo(u8"Test 8 Passed: 变换行生命周期契约（Mobility 换边 / SetParent 子表 / 销毁注销 / 摘父表）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 9: 每行数组同步不变量 + 行字节账目
    //   · 每行数组的账目由存储自列（`GetPerRowFields`），探针只是打印它
    //   · 不变量：**每个每行数组的元素数必须等于行数** —— 新增数组时漏在
    //     Allocate/Deallocate 里同步（如 T8 加 fixed_pixel/children 时）会在这里现形
    // ─────────────────────────────────────────────────────────────
    {
        auto *storage = context.GetTransformStorage();
        if (!storage)
        {
            GLogError(u8"Test 9 Failed: 无变换存储");
            return 11;
        }

        const uint32_t rows = static_cast<uint32_t>(storage->GetCount());

        // eval_order / hierarchy_depths 由 RebuildTopologyOrder 按需重建 ⇒ 先把拓扑结算一次，
        // 再验"每条平行数组都是一行一条"（结算后这条不变量必须严格成立）。
        storage->UpdateDirtyWorldMatricesFlat();

        if (const char *mismatch = storage->FindPerRowCountMismatch())
        {
            GLogError(u8"Test 9 Failed: 每行数组 '%s' 的元素数与行数（%u）不一致"
                      u8"——漏在 Allocate/Deallocate 里同步", mismatch, rows);
            return 11;
        }

        GLogInfo(u8"Test 9 Passed: 每行数组同步（%u 行 × %u 条平行数组 = 每行 %u B，其中派生/缓存 %u B）。",
                 rows,
                 static_cast<uint32_t>(TransformDataStorage::PER_ROW_FIELD_COUNT),
                 storage->PerRowBytes(),
                 storage->DerivedRowBytes());
    }

    // ─────────────────────────────────────────────────────────────
    // Test 10: 句柄失效契约（v2 约束 §2 —— revision/世代校验）
    //   · 活行的句柄有效
    //   · 行释放后，**旧句柄立刻失效**（悬垂写入窗口为零）
    //   · **死行上的新句柄也必须无效**（世代 0 = 死）
    //   · 空句柄的写入被忽略，且不得影响存储
    // ─────────────────────────────────────────────────────────────
    {
        auto *storage = context.GetTransformStorage();
        if (!storage)
        {
            GLogError(u8"Test 10 Failed: 无变换存储");
            return 12;
        }

        auto ent_live = context.CreateEntity<Entity>("HandleLiveProbe");
        const TransformID id_live = context.CreateTransform(ent_live->GetEntityID(), Mobility::Static);
        auto live = context.GetTransform(id_live);

        if (!live.IsValid())
        {
            GLogError(u8"Test 10 Failed: 活行的句柄无效");
            return 12;
        }

        const EntityID live_entity_id = ent_live->GetEntityID();
        context.DestroyEntity(live_entity_id);          // 释放该行 ⇒ 世代归 0

        if (live.IsValid())
        {
            GLogError(u8"Test 10 Failed: 行释放后旧句柄仍报有效（悬垂写入窗口）");
            return 12;
        }

        live.SetLocalPosition(glm::vec3(9.0f, 9.0f, 9.0f));     // 空句柄写入必须被忽略

        if (storage->GetPosition(id_live) != glm::vec3(0.0f))
        {
            GLogError(u8"Test 10 Failed: 空句柄的写入影响了存储");
            return 12;
        }

        // 死行上的**新**句柄同样必须无效（否则"拿到已释放行"这条路会静默生效）
        if (context.GetTransform(id_live).IsValid())
        {
            GLogError(u8"Test 10 Failed: 死行上的新句柄被判定为有效（世代 0 未生效）");
            return 12;
        }

        auto ent_new = context.CreateEntity<Entity>("HandleNewProbe");
        const TransformID id_new = context.CreateTransform(ent_new->GetEntityID(), Mobility::Static);

        if (!context.GetTransform(id_new).IsValid())
        {
            GLogError(u8"Test 10 Failed: 新行的句柄无效");
            return 12;
        }

        GLogInfo(u8"Test 10 Passed: 句柄失效契约（活柄有效 / 释放即失效 / 死行新柄也失效 / 空柄写入被忽略）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 11: 声明式行预算契约（T10/B1a）
    //   · `Initialize(static, dynamic)` 声明预算；**预算内分配行号仍是追加序**（本步不改行号含义）
    //   · 超预算 ⇒ **拒绝分配**（INVALID_HANDLE）+ 行数不变（**不扩容**）
    //   · 超预算只告警一次（告警闩），并留下可观测计数
    //   · 默认预算 = (4096, 4096)（P-2 拍板 2026-10-02）
    // ─────────────────────────────────────────────────────────────
    {
        TransformDataStorage budget_storage;      // 独立实例：不污染主世界存储

        if (budget_storage.GetStaticRowBudget()  != TransformDataStorage::kDefaultMaxStaticRows ||
            budget_storage.GetDynamicRowBudget() != TransformDataStorage::kDefaultMaxDynamicRows ||
            budget_storage.GetRowBudget()        != (TransformDataStorage::kDefaultMaxStaticRows +
                                                     TransformDataStorage::kDefaultMaxDynamicRows))
        {
            GLogError(u8"Test 11 Failed: 默认行预算不是 (static %u + dynamic %u)（实测 %u + %u）",
                      TransformDataStorage::kDefaultMaxStaticRows,
                      TransformDataStorage::kDefaultMaxDynamicRows,
                      budget_storage.GetStaticRowBudget(),
                      budget_storage.GetDynamicRowBudget());
            return 13;
        }

        budget_storage.Initialize(2,2);           // 4 行预算

        for (uint32_t i = 0; i < 4; ++i)
        {
            const TransformDataStorage::HandleID id = budget_storage.Allocate();

            if (id != i)
            {
                GLogError(u8"Test 11 Failed: 预算内分配的行号不是追加序（%u != %u）", id, i);
                return 13;
            }
        }

        const TransformDataStorage::HandleID over = budget_storage.Allocate();   // 第 5 次 ⇒ 超预算

        if (over != TransformDataStorage::INVALID_HANDLE ||
            budget_storage.GetCount() != 4 ||
            budget_storage.GetRowOverflowCount() != 1 ||
            !budget_storage.HasWarnedRowOverflow())
        {
            GLogError(u8"Test 11 Failed: 超预算未拒绝分配（id=%u rows=%d overflow=%u warned=%d）"
                      u8"—— 预算制要求「拒绝 + 一次性告警 + 不扩容」",
                      over,
                      budget_storage.GetCount(),
                      budget_storage.GetRowOverflowCount(),
                      budget_storage.HasWarnedRowOverflow() ? 1 : 0);
            return 13;
        }

        budget_storage.Allocate();                // 再来一次：计数增、行数不变

        if (budget_storage.GetRowOverflowCount() != 2 || budget_storage.GetCount() != 4)
        {
            GLogError(u8"Test 11 Failed: 二次超预算仍在扩容（rows=%d overflow=%u）",
                      budget_storage.GetCount(),
                      budget_storage.GetRowOverflowCount());
            return 13;
        }

        GLogInfo(u8"Test 11 Passed: 声明式行预算契约（默认 4096+4096 / 预算内为追加序 / 超限拒绝分配不扩容 + 一次性告警闩）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 12: B1a 禁复活源码契约（数据驱动扩容/重建路径不得回仓）
    //   背景：`StatTransform` 按需扩容 + `EnsureTransformIndexRowsCapacity` 重建 +
    //   `"L2W recreated"` 日志 = T10 要消灭的"运行期行号/地址移动"。删掉之后
    //   只要有人把任一环写回来，本测试立刻红（而不是靠肉眼看日志）。
    // ─────────────────────────────────────────────────────────────
    {
        auto read_source = [](const char *path, std::string &out) -> bool
        {
            std::ifstream in(path, std::ios::binary);

            if (!in)
                return false;

            out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            return true;
        };

        struct SourceNeedle
        {
            const char *path;
            const char *needle;
            uint32_t    count;
            const char *why;
        };

        const SourceNeedle kNeedles[] =
        {
            // L2W 一侧：动态扩容/重建路径**必须零残留**
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "StatTransform",                   0, "L2W 按需扩容的旧入口复活（T10：预算期一次性分配）" },
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "EnsureTransformIndexRowsCapacity",0, "index 行表按需重建复活" },
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "L2W recreated",                    0, "运行期重建 L2W 的日志/路径复活（行号/地址会移动）" },
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "transform_policy",                 0, "按策略重建 buffer 的旧字段复活" },
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "power_to_2",                       0, "按需取 2 次幂容量的旧写法复活（容量应来自预算）" },
            { "inc/hgl/ecs/support/TransformAssignmentBuffer.h", "EnsureCapacity",                 0, "旧入口名复活（每帧入口应为 EnsureBudget）" },

            // 预算期一次性分配与预算字段必须在位
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "AllocateBudgetBuffers",            2, "预算期一次性分配入口缺失（声明 + 定义各一次）" },
            { "src/ecs/support/TransformAssignmentBuffer.cpp", "EnsureBudget",                     2, "每帧预算入口缺失（定义 + 注释各一次）" },
            { "inc/hgl/ecs/support/TransformAssignmentBuffer.h", "max_static_rows",               2, "预算字段缺失（容量不再由预算决定）" },

            // 存储侧：声明式预算与"拒绝分配"必须在位
            { "inc/hgl/ecs/support/TransformDataStorage.h", "kDefaultMaxStaticRows",               3, "默认预算常量缺失（P-2 拍板 static 4096 + dynamic 4096）" },
            { "inc/hgl/ecs/support/TransformDataStorage.h", "GetRowBudget",                        4, "行预算访问器缺失（访问器 + Initialize + Assemble 校验 + 告警参数）" },
            { "inc/hgl/ecs/support/TransformDataStorage.h", "row_overflow_warned",                 5, "超限一次性告警闩缺失（字段 + Initialize 复位 + 判定 + 置位 + 访问器）" },

            // 调用点：必须走预算入口
            { "src/ecs/systems/tick/TransformSystem.cpp",     "EnsureBudget(",                     1, "每帧入口未走预算 API" },
            { "src/ecs/support/line/LineRenderPipeline.cpp",  "EnsureBudget(",                     1, "Line 侧每帧入口未走预算 API" },

            // B1b：L2W 行号必须来自**稳定槽位**（禁复活"名单排名"语义）
            { "src/ecs/systems/tick/TransformSystem.cpp", "TryGetTransformGroupIndex", 0, "\"名单排名\"式索引查询复活（行号必须来自稳定槽位）" },
            { "inc/hgl/ecs/systems/tick/TransformSystem.h", "TryGetTransformGroupIndex", 0, "同上（声明）" },
            { "src/ecs/systems/tick/TransformSystem.cpp", "static_index_map",          0, "排名映射复活（应为 static_slot_map）" },
            { "src/ecs/systems/tick/TransformSystem.cpp", "dynamic_index_map",         0, "同上（动态段）" },
            { "inc/hgl/ecs/core/Context.h",               "TryGetTransformSlot",       1, "稳定槽位查询缺失" },
            // 批 1.3 C6：遍历统一走 `ForEach*Transform` 后，槽号改由 slot_map 反查（2 处遍历）
            // ⇒ 该符号用点由 4 涨到 6；针的意图（每帧派生自世界槽位表、不许退化成排名映射）不变。
            { "src/ecs/systems/tick/TransformSystem.cpp", "static_slot_map",           6, "槽位映射缺失（每帧派生自世界槽位表）" },
            { "src/ecs/support/TransformAccessor.cpp",    "MigrateTransform",          1, "换边未通知世界（只改 mobility 字节 ⇒ 实例拿默认行、全画在原点）" },

            // T8.5：局部矩阵常驻缓存与二级标脏字段**必须零残留**（行内存 223 B → 158 B 的验收点）
            { "inc/hgl/ecs/support/TransformDataStorage.h", "local_matrices",            0, "局部矩阵常驻缓存复活（真源只剩 TRS + 求值循环内按需合成）" },
            { "inc/hgl/ecs/support/TransformDataStorage.h", "local_dirty",               0, "第二级标脏字段复活（只留单级 matrixDirty）" },
        };

        for (const SourceNeedle &n : kNeedles)
        {
            std::string src;

            if (!read_source(n.path, src))
            {
                GLogError(u8"Test 12 Failed: cannot open %s (run from repo root)", n.path);
                return 14;
            }

            size_t pos = 0;
            uint32_t hit = 0;

            while ((pos = src.find(n.needle, pos)) != std::string::npos)
            {
                ++hit;
                pos += std::strlen(n.needle);
            }

            if (hit != n.count)
            {
                GLogError(u8"Test 12 Failed: %s 里 '%s' 出现 %u 次（期望 %u）—— %s",
                          n.path, n.needle, hit, n.count, n.why);
                return 14;
            }
        }

        GLogInfo(u8"Test 12 Passed: B1a 禁复活契约成立（L2W 动态扩容/重建路径 0 残留、预算期一次性分配与声明式预算在位、两个每帧入口均走 EnsureBudget；共 %u 条）。",
                 static_cast<uint32_t>(sizeof(kNeedles) / sizeof(kNeedles[0])));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 13: 稳定槽位契约（B1b / T10：行号不再随名单顺序漂移）
    //   · 槽位一经分配即稳定 —— **销毁中间一个物体不影响其它物体的槽位**（改前 erase 会整体平移）
    //   · 空洞优先复用 ⇒ 新物体拿回被释放的槽位（= 拿回旧行号）
    //   · Mobility 换边 = 换段（旧槽位归还 + 新段新槽位）+ 移动计数增
    //   · 段预算用尽 ⇒ 拒绝：`CreateTransform` 返 INVALID_TRANSFORM_ID、活槽数不变
    // ─────────────────────────────────────────────────────────────
    {
        auto *storage = context.GetTransformStorage();

        if (!storage)
        {
            GLogError(u8"Test 13 Failed: 无变换存储");
            return 15;
        }

        auto ent_a = context.CreateEntity<Entity>("SlotA");
        auto ent_b = context.CreateEntity<Entity>("SlotB");
        auto ent_c = context.CreateEntity<Entity>("SlotC");

        const TransformID id_a = context.CreateTransform(ent_a->GetEntityID(), Mobility::Static);
        const TransformID id_b = context.CreateTransform(ent_b->GetEntityID(), Mobility::Static);
        const TransformID id_c = context.CreateTransform(ent_c->GetEntityID(), Mobility::Static);

        uint32_t slot_a = 0;
        uint32_t slot_b = 0;
        uint32_t slot_c = 0;

        if (!IsValidTransformID(id_a) || !IsValidTransformID(id_b) || !IsValidTransformID(id_c) ||
            !context.TryGetTransformSlot(id_a,false,slot_a) ||
            !context.TryGetTransformSlot(id_b,false,slot_b) ||
            !context.TryGetTransformSlot(id_c,false,slot_c))
        {
            GLogError(u8"Test 13 Failed: 三个静态变换的槽位分配/查询失败");
            return 15;
        }

        if (slot_a == slot_b || slot_b == slot_c || slot_a == slot_c)
        {
            GLogError(u8"Test 13 Failed: 槽位重复（%u / %u / %u）", slot_a, slot_b, slot_c);
            return 15;
        }

        // 销毁中间那个 ⇒ 其它两个的槽位必须**一格不动**（这就是 T10 的"行号冻结"判据）
        context.DestroyEntity(ent_b->GetEntityID());

        uint32_t slot_a_after = 0;
        uint32_t slot_c_after = 0;

        if (!context.TryGetTransformSlot(id_a,false,slot_a_after) || slot_a_after != slot_a ||
            !context.TryGetTransformSlot(id_c,false,slot_c_after) || slot_c_after != slot_c)
        {
            GLogError(u8"Test 13 Failed: 销毁中间物体后其它物体槽位漂移（A %u→%u / C %u→%u）"
                      u8"—— 行号必须由稳定槽位决定，不得按名单顺序重排",
                      slot_a, slot_a_after, slot_c, slot_c_after);
            return 15;
        }

        // 新物体复用空洞 ⇒ 拿回被释放的槽位
        auto ent_d = context.CreateEntity<Entity>("SlotD");
        const TransformID id_d = context.CreateTransform(ent_d->GetEntityID(), Mobility::Static);
        uint32_t slot_d = 0;

        if (!IsValidTransformID(id_d) || !context.TryGetTransformSlot(id_d,false,slot_d) || slot_d != slot_b)
        {
            GLogError(u8"Test 13 Failed: 新物体未复用被释放的槽位（期望 %u，实际 %u）", slot_b, slot_d);
            return 15;
        }

        // Mobility 换边 ⇒ 换段（旧段查询失败 + 新段拿到槽位）+ 计数增
        const uint32_t move_before = context.GetTransformRowMoveCount();
        context.MigrateTransform(id_a,true);

        uint32_t slot_a_movable = 0;
        uint32_t slot_a_static_dummy = 0;

        if (context.TryGetTransformSlot(id_a,false,slot_a_static_dummy) ||
            !context.TryGetTransformSlot(id_a,true,slot_a_movable) ||
            context.GetTransformRowMoveCount() != move_before + 1)
        {
            GLogError(u8"Test 13 Failed: Mobility 换边未换段（static 命中=%d movable 命中=%d 计数 %u→%u）",
                      context.TryGetTransformSlot(id_a,false,slot_a_static_dummy) ? 1 : 0,
                      context.TryGetTransformSlot(id_a,true,slot_a_movable) ? 1 : 0,
                      move_before, context.GetTransformRowMoveCount());
            return 15;
        }

        // 段预算用尽 ⇒ 拒绝（不扩容；返回 INVALID_TRANSFORM_ID，活槽数不变）
        storage->Initialize(2,2);

        auto ent_over = context.CreateEntity<Entity>("SlotOverBudget");
        const uint32_t live_before = context.GetLiveTransformCount(false);
        const TransformID id_over = context.CreateTransform(ent_over->GetEntityID(), Mobility::Static);

        if (IsValidTransformID(id_over) || context.GetLiveTransformCount(false) != live_before)
        {
            GLogError(u8"Test 13 Failed: 段预算用尽仍在分配（id 有效=%d 活槽 %u→%u）",
                      IsValidTransformID(id_over) ? 1 : 0, live_before, context.GetLiveTransformCount(false));
            return 15;
        }

        storage->Initialize(TransformDataStorage::kDefaultMaxStaticRows,
                            TransformDataStorage::kDefaultMaxDynamicRows);

        GLogInfo(u8"Test 13 Passed: 稳定槽位契约（销毁不影响他者槽位 / 空洞复用 / 换边换段并计数 / 段预算用尽拒绝分配）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 14: B2 稀疏侧表契约
    //   · fixed-pixel 状态是真源在存储（旧实现 32 B/行 × 全表 ⇒ 改稀疏 map）
    //   · 只有被配置过的行才有状态：**未配置的行必须读到默认（关闭）**，不得串味
    //   · 子表 CSR：`GetChildCount/GetChild/HasChild/CopyChildren` 与 Add/Remove 一致
    // ─────────────────────────────────────────────────────────────
    {
        auto *storage = context.GetTransformStorage();

        if (!storage)
        {
            GLogError(u8"Test 14 Failed: 无变换存储");
            return 16;
        }

        auto ent_fp = context.CreateEntity<Entity>("FixedPixelProbe");
        auto ent_plain = context.CreateEntity<Entity>("PlainProbe");

        const TransformID fp_id = context.CreateTransform(ent_fp->GetEntityID(), Mobility::Static);
        const TransformID plain_id = context.CreateTransform(ent_plain->GetEntityID(), Mobility::Static);

        auto fp = context.GetTransform(fp_id);
        auto plain = context.GetTransform(plain_id);

        if (fp.IsFixedPixelSizingEnabled() || plain.IsFixedPixelSizingEnabled())
        {
            GLogError(u8"Test 14 Failed: 未配置的行不得报 fixed-pixel 已启用（稀疏表串味）");
            return 16;
        }

        fp.SetFixedPixelSizingParameters(320.0f,2.0f,0.05f);
        fp.SetFixedPixelSizingEnabled(true);

        if (!fp.IsFixedPixelSizingEnabled())
        {
            GLogError(u8"Test 14 Failed: 配置后 fixed-pixel 未启用（稀疏侧表写入丢失）");
            return 16;
        }

        if (plain.IsFixedPixelSizingEnabled())
        {
            GLogError(u8"Test 14 Failed: 另一行的 fixed-pixel 被相邻行污染");
            return 16;
        }

        // 子表 CSR 往返：Add → 读回 → 查找 → Remove
        const TransformID csr_child_a = context.CreateTransform(context.CreateEntity<Entity>("CsrChildA")->GetEntityID(), Mobility::Static);
        const TransformID csr_child_b = context.CreateTransform(context.CreateEntity<Entity>("CsrChildB")->GetEntityID(), Mobility::Static);

        storage->AddChild(fp_id,csr_child_a);
        storage->AddChild(fp_id,csr_child_b);
        storage->AddChild(fp_id,csr_child_a);          // 重复登记必须幂等

        if (storage->GetChildCount(fp_id) != 2 ||
            storage->GetChild(fp_id,0) != csr_child_a ||
            storage->GetChild(fp_id,1) != csr_child_b ||
            !storage->HasChild(fp_id,csr_child_a) ||
            storage->HasChild(fp_id,csr_child_b + 999u))
        {
            GLogError(u8"Test 14 Failed: 子表 CSR 读写不一致（count=%u）", storage->GetChildCount(fp_id));
            return 16;
        }

        std::vector<TransformDataStorage::HandleID> snapshot;
        storage->CopyChildren(fp_id,snapshot);

        if (snapshot.size() != 2 || snapshot[0] != csr_child_a || snapshot[1] != csr_child_b)
        {
            GLogError(u8"Test 14 Failed: CopyChildren 快照与 CSR 区间不一致");
            return 16;
        }

        storage->RemoveChild(fp_id,csr_child_a);

        if (storage->GetChildCount(fp_id) != 1 || storage->HasChild(fp_id,csr_child_a) ||
            storage->GetChild(fp_id,0) != csr_child_b)
        {
            GLogError(u8"Test 14 Failed: RemoveChild 后行内区间未保持连续");
            return 16;
        }

        // 释放行 ⇒ 稀疏表条目必须摘除
        context.DestroyEntity(ent_fp->GetEntityID());

        if (storage->GetFixedPixel(fp_id).enabled)
        {
            GLogError(u8"Test 14 Failed: 行释放后 fixed-pixel 条目未摘除（稀疏表泄漏）");
            return 16;
        }

        GLogInfo(u8"Test 14 Passed: B2 稀疏侧表契约（fixed-pixel 按行隔离/释放即摘除；子表 CSR 增删查与快照一致）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 15: 名单空洞安全契约（2026-10-02，结构性修复）
    //
    // 背景：变换名单里存在**空洞**（销毁 / Mobility 换边留下 `INVALID_TRANSFORM_ID`，见 Context.h）。
    // 拿空洞去查询行存储 = 以 (uint32)-1 索引平行数组 ⇒ 越界崩溃。实测栈：
    //   TransformSystem::Update → TransformDataStorage::IsDirty(4294967295)
    //   → ValueArray<unsigned char>::operator[](-1) → std::vector OOB。
    // 本用例钉住两层：① 行存储入口对无效句柄安全返回（不崩）；
    //                ② `ECSContext::ForEach*Transform` 结构性遍历自动跳洞、计数 = 活槽数。
    // ─────────────────────────────────────────────────────────────
    {
        auto *hole_storage = context.GetTransformStorage();

        if (!hole_storage)
        {
            GLogError(u8"Test 15 Failed: 无变换存储");
            return 17;
        }

        // ① 行存储入口：无效句柄不得越界（曾崩在这里）
        if (hole_storage->IsDirty(TransformDataStorage::INVALID_HANDLE))
        {
            GLogError(u8"Test 15 Failed: IsDirty(INVALID_HANDLE) 返回 true（应视为不脏并不崩）");
            return 17;
        }

        // 制造空洞：movable 变换随实体销毁 ⇒ 名单留下 INVALID_TRANSFORM_ID
        auto *hole_ent = context.CreateEntity<Entity>("HoleProbe");
        const TransformID hole_id = context.CreateTransform(hole_ent->GetEntityID(), Mobility::Movable);

        if (!IsValidTransformID(hole_id))
        {
            GLogError(u8"Test 15 Failed: 创建 movable 变换失败（段预算用尽？用例前提不成立）");
            return 17;
        }

        context.DestroyEntity(hole_ent->GetEntityID());

        bool has_hole = false;
        for (const TransformID id : context.GetMovableTransforms())
            if (!IsValidTransformID(id))
                has_hole = true;

        if (!has_hole)
        {
            GLogError(u8"Test 15 Failed: 销毁 movable 实体后名单未留空洞（前提变化，用例需更新）");
            return 17;
        }

        // ② 结构性遍历：必须跳过空洞
        uint32_t iterated = 0;
        uint32_t invalid_seen = 0;

        context.ForEachMovableTransform([&](TransformID id)
        {
            ++iterated;

            if (!IsValidTransformID(id))
                ++invalid_seen;
        });

        if (invalid_seen != 0)
        {
            GLogError(u8"Test 15 Failed: ForEachMovableTransform 把空洞传给了回调（%u 次）", invalid_seen);
            return 17;
        }

        if (iterated != context.GetLiveTransformCount(true))
        {
            GLogError(u8"Test 15 Failed: ForEach 计数 %u ≠ 活槽数 %u",
                      iterated, context.GetLiveTransformCount(true));
            return 17;
        }

        GLogInfo(u8"Test 15 Passed: 名单空洞安全契约成立（IsDirty(INVALID_HANDLE) 安全返回 false 不崩；"
                 u8"ForEachMovableTransform 跳过空洞且计数 %u == 活槽数 %u）。",
                 iterated, context.GetLiveTransformCount(true));
    }

    // ─────────────────────────────────────────────────────────────
    // Test 16: CSR 子表搬迁契约（审计 · 批 1.1 B1）
    //   · 交错追加（父 P 有子 → 另一父 Q 有子 → P 再加子）必须把 P 的旧子节点**整段复制**到池尾
    //   · 删除后再追加（span 不再贴池尾）同样必须复制
    //   · count / 顺序 / HasChild / CopyChildren 四者必须一致（原实现丢内容 + 可越界读）
    // ─────────────────────────────────────────────────────────────
    {
        TransformDataStorage csr;

        const TransformDataStorage::HandleID p = csr.Allocate();
        const TransformDataStorage::HandleID q = csr.Allocate();
        const TransformDataStorage::HandleID a = csr.Allocate();
        const TransformDataStorage::HandleID b = csr.Allocate();
        const TransformDataStorage::HandleID c = csr.Allocate();

        csr.AddChild(p,a);
        csr.AddChild(q,b);                 // 让 p 的 span 不再贴着池尾
        csr.AddChild(p,c);                 // ← 交错追加：必须复制 [a]

        if (csr.GetChildCount(p) != 2 || csr.GetChild(p,0) != a || csr.GetChild(p,1) != c)
        {
            GLogError(u8"Test 16 Failed: 交错追加后 P 的子表不对（count=%u first=%u second=%u，期望 2/%u/%u）",
                      csr.GetChildCount(p), csr.GetChild(p,0), csr.GetChild(p,1), a, c);
            return 18;
        }

        if (!csr.HasChild(p,a) || !csr.HasChild(p,c) || csr.HasChild(p,b))
        {
            GLogError(u8"Test 16 Failed: HasChild 不一致（a=%d c=%d b=%d，期望 1/1/0）",
                      csr.HasChild(p,a), csr.HasChild(p,c), csr.HasChild(p,b));
            return 18;
        }

        std::vector<TransformDataStorage::HandleID> kids;
        csr.CopyChildren(p,kids);

        if (kids.size() != 2 || kids[0] != a || kids[1] != c)
        {
            GLogError(u8"Test 16 Failed: CopyChildren 结果不对（size=%zu [0]=%u [1]=%u）",
                      kids.size(), kids.empty() ? 0u : kids[0], kids.size() < 2 ? 0u : kids[1]);
            return 18;
        }

        // 删除后再追加：RemoveChild 行内前移保持连续，span 不再贴池尾 ⇒ 同样走复制路径
        csr.RemoveChild(p,a);
        csr.AddChild(p,a);
        csr.CopyChildren(p,kids);

        if (kids.size() != 2 || kids[0] != c || kids[1] != a)
        {
            GLogError(u8"Test 16 Failed: 删除后追加的子表不对（size=%zu [0]=%u [1]=%u，期望 2/%u/%u）",
                      kids.size(), kids.empty() ? 0u : kids[0], kids.size() < 2 ? 0u : kids[1], c, a);
            return 18;
        }

        GLogInfo(u8"Test 16 Passed: CSR 子表搬迁契约（交错追加 / 删除后追加都整段复制旧子节点；count/顺序/查找/拷贝一致）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 17: 父环拒绝 + 后代遍历可终止（审计 · 批 1.1 B2）
    //   · 经公共 API（accessor）设父时，祖先环必须在改父链之前被拒绝
    //   · 拒绝后父链保持不变、且调用**必须返回**（原先 A↔B 互指会让后代标脏死循环）
    // ─────────────────────────────────────────────────────────────
    {
        auto ent_w = context.CreateEntity<Entity>("CycleW");
        auto ent_x = context.CreateEntity<Entity>("CycleX");
        auto ent_y = context.CreateEntity<Entity>("CycleY");

        const TransformID id_w = context.CreateTransform(ent_w->GetEntityID(), Mobility::Static);
        const TransformID id_x = context.CreateTransform(ent_x->GetEntityID(), Mobility::Static);
        const TransformID id_y = context.CreateTransform(ent_y->GetEntityID(), Mobility::Static);

        if (!IsValidTransformID(id_w) || !IsValidTransformID(id_x) || !IsValidTransformID(id_y))
        {
            GLogError(u8"Test 17 Failed: 创建三个静态变换失败（段预算用尽？用例前提不成立）");
            return 19;
        }

        auto tw = context.GetTransform(id_w);
        auto tx = context.GetTransform(id_x);
        auto ty = context.GetTransform(id_y);

        tw.SetParent(id_x);                       // X → W
        tx.SetParent(id_y);                       // Y → X → W

        if (tw.GetParent() != id_x || tx.GetParent() != id_y)
        {
            GLogError(u8"Test 17 Failed: 正常设父失败（W.parent=%u X.parent=%u）",
                      static_cast<uint32_t>(tw.GetParent()), static_cast<uint32_t>(tx.GetParent()));
            return 19;
        }

        const TransformID w_parent_before = tw.GetParent();

        ty.SetParent(id_w);                       // W → Y 会成环（Y → X → W）⇒ 必须被拒绝且必须返回

        if (tw.GetParent() != w_parent_before || tw.GetParent() == id_y)
        {
            GLogError(u8"Test 17 Failed: 成环的 SetParent 未被拒绝（W.parent 现在=%u）",
                      static_cast<uint32_t>(tw.GetParent()));
            return 19;
        }

        if (ty.GetParent() != INVALID_TRANSFORM_ID)
        {
            GLogError(u8"Test 17 Failed: 被拒的行父链被改动（Y.parent=%u，期望无效）",
                      static_cast<uint32_t>(ty.GetParent()));
            return 19;
        }

        // 拒绝后仍能正常改父（无残留半改状态）。
        // 注意：此刻 X 的父正是 Y ⇒ 「Y 挂到 X 下」本身就是 2-环，必须先摘掉 X→Y 这条边。
        tx.SetParent(INVALID_TRANSFORM_ID);

        if (tx.GetParent() != INVALID_TRANSFORM_ID)
        {
            GLogError(u8"Test 17 Failed: 摘父失败（X.parent=%u）",
                      static_cast<uint32_t>(tx.GetParent()));
            return 19;
        }

        ty.SetParent(id_x);

        if (ty.GetParent() != id_x)
        {
            GLogError(u8"Test 17 Failed: 被拒后合法设父失效（Y.parent=%u，期望 %u）",
                      static_cast<uint32_t>(ty.GetParent()), static_cast<uint32_t>(id_x));
            return 19;
        }

        // 后代标脏传播必须终止（这一步在修复前会死循环 —— 用例本身完成即是判据）
        tx.SetLocalPosition(glm::vec3(1.0f, 1.0f, 1.0f));

        GLogInfo(u8"Test 17 Passed: 父环拒绝契约成立（祖先环被拒 / 父链保持不变 / 调用返回 / 拒绝后仍可正常设父）。");
    }

        // ─────────────────────────────────────────────────────────────────────
        // Test 18: 段预算拒绝的原子性（审计 · 批 1.1 B3）
        //   ① 换段被拒（目标段满）必须**整体拒绝**：源段槽保持、目标段已有行的槽位不得被挤掉
        //      （原实现的失败分支写「目标段 + 旧下标」⇒ 挤掉别人的槽位，old_slot 超出目标段
        //       长度时还会越界写）、换边计数与 storage 的 mobility 真源都不得被改动。
        //   ② 段满时的 CreateTransform 不得返回半成品：登记失败必须回收已分配的行并返回 INVALID
        //      （原实现丢弃登记结果 ⇒ 返回"有效但拿不到 L2W 槽"的行）。
        // ─────────────────────────────────────────────────────────────────────
        {
            // ① 换段被拒：静态段 1 槽 / 动态段 1 槽 ⇒ 两边都只剩 0 个空位
            ECSContext ctx_full;

            auto *st_full = ctx_full.GetTransformStorage();

            if (!st_full)
            {
                GLogError(u8"Test 18 Failed: 无变换存储");
                return 20;
            }

            st_full->Initialize(1, 1);

            auto e_s = ctx_full.CreateEntity<Entity>("SegStatic");
            auto e_d = ctx_full.CreateEntity<Entity>("SegDynamic");

            const TransformID id_s = ctx_full.CreateTransform(e_s->GetEntityID(), Mobility::Static);
            const TransformID id_d = ctx_full.CreateTransform(e_d->GetEntityID(), Mobility::Movable);

            if (!IsValidTransformID(id_s) || !IsValidTransformID(id_d))
            {
                GLogError(u8"Test 18 Failed: 段预算内创建变换失败（static=%u movable=%u）",
                          static_cast<uint32_t>(id_s), static_cast<uint32_t>(id_d));
                return 20;
            }

            const uint32_t moves_before = ctx_full.GetTransformRowMoveCount();
            const uint32_t live_static_before = ctx_full.GetLiveTransformCount(false);

            ctx_full.GetTransform(id_s).SetMobility(Mobility::Movable);   // 目标段已满 ⇒ 必须整体拒绝

            uint32_t slot_probe = 0;

            if (!ctx_full.TryGetTransformSlot(id_s, false, slot_probe))
            {
                GLogError(u8"Test 18 Failed: 被拒的换边把该行从源段摘掉了（源段查不到 id=%u）",
                          static_cast<uint32_t>(id_s));
                return 20;
            }

            if (ctx_full.TryGetTransformSlot(id_s, true, slot_probe))
            {
                GLogError(u8"Test 18 Failed: 目标段满却仍完成了换边（id=%u 出现在动态段）",
                          static_cast<uint32_t>(id_s));
                return 20;
            }

            uint32_t slot_d_after = 99;

            if (!ctx_full.TryGetTransformSlot(id_d, true, slot_d_after) || slot_d_after != 0)
            {
                GLogError(u8"Test 18 Failed: 目标段原有行的槽位被被拒的换边挤掉（动态槽=%u，期望 0）",
                          slot_d_after);
                return 20;
            }

            if (ctx_full.GetTransformRowMoveCount() != moves_before)
            {
                GLogError(u8"Test 18 Failed: 被拒的换边仍计入换边次数（%u → %u）",
                          moves_before, ctx_full.GetTransformRowMoveCount());
                return 20;
            }

            if (ctx_full.GetLiveTransformCount(false) != live_static_before)
            {
                GLogError(u8"Test 18 Failed: 被拒的换边改了段名单（静态活行 %u → %u）",
                          live_static_before, ctx_full.GetLiveTransformCount(false));
                return 20;
            }

            if (ctx_full.GetTransform(id_s).IsMovable())
            {
                GLogError(u8"Test 18 Failed: 被拒的换边改了 storage 的 mobility 真源（应为静态）");
                return 20;
            }

            // ② 段满创建：总行预算仍有空间（静态 1 + 动态 3 = 4），只有静态段满
            ECSContext ctx_segfull;

            auto *st_seg = ctx_segfull.GetTransformStorage();

            if (!st_seg)
            {
                GLogError(u8"Test 18 Failed: 无变换存储");
                return 20;
            }

            st_seg->Initialize(1, 3);

            auto e_s2 = ctx_segfull.CreateEntity<Entity>("SegFullStatic");

            if (!IsValidTransformID(ctx_segfull.CreateTransform(e_s2->GetEntityID(), Mobility::Static)))
            {
                GLogError(u8"Test 18 Failed: 静态段首个变换创建失败");
                return 20;
            }

            const int rows_before = st_seg->GetCount();

            auto e_over = ctx_segfull.CreateEntity<Entity>("SegOverflow");
            const TransformID id_over = ctx_segfull.CreateTransform(e_over->GetEntityID(), Mobility::Static);

            if (IsValidTransformID(id_over))
            {
                GLogError(u8"Test 18 Failed: 静态段满仍返回有效变换 id=%u（半成品：有行无 L2W 槽）",
                          static_cast<uint32_t>(id_over));
                return 20;
            }

            // 回收判据：Allocate 总是**追加**（无空闲行复用），所以被拒时那一行应停在
            // 下标 rows_before 且**世代归零**（Deallocate 的换代语义）；泄漏则它仍是活行。
            if (st_seg->GetCount() != rows_before + 1
             || st_seg->GetGeneration(static_cast<TransformDataStorage::HandleID>(rows_before)) != 0)
            {
                GLogError(u8"Test 18 Failed: 段满创建失败后行未回收（rows %d → %d，行世代=%u）",
                          rows_before, st_seg->GetCount(),
                          st_seg->GetGeneration(static_cast<TransformDataStorage::HandleID>(rows_before)));
                return 20;
            }

            // 失败不得留下"占了槽却查不到"的账：动态段随后必须能正常建行
            auto e_dyn = ctx_segfull.CreateEntity<Entity>("SegAfterFail");
            const TransformID id_dyn = ctx_segfull.CreateTransform(e_dyn->GetEntityID(), Mobility::Movable);

            if (!IsValidTransformID(id_dyn) || ctx_segfull.GetLiveTransformCount(true) != 1)
            {
                GLogError(u8"Test 18 Failed: 失败路径污染了段账目（动态建行=%u，动态活行=%u）",
                          static_cast<uint32_t>(id_dyn), ctx_segfull.GetLiveTransformCount(true));
                return 20;
            }

            GLogInfo(u8"Test 18 Passed: 段预算拒绝的原子性成立（换边整体拒绝 / 目标段槽位不被挤 / "
                      u8"计数与 mobility 真源不动 / 段满创建回收行不造半成品）。");
        }

    // ─────────────────────────────────────────────────────────────────────
    // Test 19: 世界复位契约（§2 #9）
    //   ① 复位（`TransformDataStorage::Clear()`）**不复位世代**：复位前取得的访问器必须失效
    //      （原实现 Clear 清空 generations、新行又从常量 1 起编号 ⇒ 旧访问器"复活"）
    //   ② `ECSContext::Shutdown` 必须复位变换行存储（原实现只清实体与名单 ⇒ 新实体继承旧变换行）
    // ─────────────────────────────────────────────────────────────────────
    {
        ECSContext ctx_reset;

        auto e1 = ctx_reset.CreateEntity<Entity>("ResetOwner1");

        const TransformID id1 = ctx_reset.CreateTransform(e1->GetEntityID(), Mobility::Static);

        auto *st_reset = ctx_reset.GetTransformStorage();

        if (!IsValidTransformID(id1) || !st_reset)
        {
            GLogError(u8"Test 19 Failed: 前置创建失败（id=%u storage=%p）",
                      static_cast<uint32_t>(id1), static_cast<void *>(st_reset));
            return 21;
        }

        const uint32_t gen1 = st_reset->GetGeneration(static_cast<TransformDataStorage::HandleID>(id1));

        const TransformAccessor acc_old = ctx_reset.GetTransform(id1);      // 建柄时捕获 gen1

        if (!acc_old.IsValid())
        {
            GLogError(u8"Test 19 Failed: 新建访问器本该有效");
            return 21;
        }

        st_reset->Clear();                                                  // Shutdown 走的就是这条路

        auto e2 = ctx_reset.CreateEntity<Entity>("ResetOwner2");

        const TransformID id2 = ctx_reset.CreateTransform(e2->GetEntityID(), Mobility::Static);

        if (!IsValidTransformID(id2))
        {
            GLogError(u8"Test 19 Failed: 复位后新建变换失败");
            return 21;
        }

        const uint32_t gen2 = st_reset->GetGeneration(static_cast<TransformDataStorage::HandleID>(id2));

        if (gen2 == gen1)
        {
            GLogError(u8"Test 19 Failed: 复位后新行的世代与旧行相同（gen=%u）⇒ 世代计数器被复位了", gen2);
            return 21;
        }

        if (acc_old.IsValid())
        {
            GLogError(u8"Test 19 Failed: 复位前取得的访问器在【复位 + 重分配】后仍然有效（句柄复活）");
            return 21;
        }

        // ② Shutdown 复位行存储（未 Initialize 的世界走 inactive 早退分支，也要复位）
        ECSContext ctx_shutdown;

        auto e3 = ctx_shutdown.CreateEntity<Entity>("ShutdownOwner");

        if (!IsValidTransformID(ctx_shutdown.CreateTransform(e3->GetEntityID(), Mobility::Movable)))
        {
            GLogError(u8"Test 19 Failed: Shutdown 前置创建失败");
            return 21;
        }

        ctx_shutdown.Shutdown();

        if (auto *st_after = ctx_shutdown.GetTransformStorage())
        {
            if (st_after->GetCount() != 0)
            {
                GLogError(u8"Test 19 Failed: Shutdown 后变换行存储未复位（rows=%d）⇒ 新实体继承旧变换行",
                          st_after->GetCount());
                return 21;
            }
        }
        else
        {
            GLogInfo(u8"Test 19: Shutdown 已整体释放变换存储（等价于复位）");
        }

        GLogInfo(u8"Test 19 Passed: 世界复位契约成立（世代不复位 ⇒ 旧句柄不复活；Shutdown 复位行存储）。");
    }

    GLogInfo(u8"=== All TransformDataStorage tests passed successfully! ===");
    return 0;
}