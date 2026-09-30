#include <hgl/ecs/support/TransformDataStorage.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/support/TransformAccessor.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace hgl;
using namespace hgl::ecs;

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

        if (!contains(storage->GetChildren(id_p), id_c))
        {
            GLogError(u8"Test 8 Failed: SetParent 未登记子表（子级不会被标脏/重传）");
            return 10;
        }

        context.GetTransform(id_c).SetParent(INVALID_TRANSFORM_ID);

        if (contains(storage->GetChildren(id_p), id_c))
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

        if (!contains(storage->GetChildren(id_p2), id_c2))
        {
            GLogError(u8"Test 8 Failed: 第二次 SetParent 未登记子表");
            return 10;
        }

        const EntityID child2_entity_id = ent_c2->GetEntityID();
        context.DestroyEntity(child2_entity_id);

        if (contains(storage->GetChildren(id_p2), id_c2))
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

    GLogInfo(u8"=== All TransformDataStorage tests passed successfully! ===");
    return 0;
}