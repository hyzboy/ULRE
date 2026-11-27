#include <hgl/ecs/support/cull/VisibilityDataStorage.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

/**
 * 可见性真值契约（可见性是实体级唯一真值；组件级副本与其系统已删除）
 *
 *   1) 真值唯一：只有世界私有的 VisibilityDataStorage，默认可见；
 *   2) 祖先继承：祖先不可见 ⇒ 后代不可见（后代自身并非"直接"不可见）；
 *   3) 销毁回收：实体销毁必须从不可见集合摘掉，否则同索引的新实体会继承旧状态（T8 同类副作用）。
 *
 * 反证：抽掉 `ECSContext::DestroyEntity` 里的 `visibility_storage->SetVisible(id);` ⇒ Test 3 必须失败。
 *
 *   4) 镜像位写入纪律（2026-10-14；(b) 落地后本函数**每帧**都会被调）：`EntityGPUTable::SetVisible`
 *      必须**值变才标脏**，否则静态实体的行永远脏 ⇒ 每帧整表上传。
 *      反证：去掉该守卫（无条件 `MarkDirty`）⇒ Test 4 必须失败。
 */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    // 没有这一行时本测试的所有 GLog（含失败原因）都无处可去——静默通过/静默失败
    hgl::logger::InitLogger(OS_TEXT("TestVisibilityStorage"));

    GLogInfo(u8"=== Testing 可见性真值（世界私有 VisibilityDataStorage）===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: 默认可见 + 自身往返（真值唯一在世界存储）
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("VisWorld1");

        auto *entity = world.CreateEntity<Entity>("VisEntity");
        if (!entity)
        {
            GLogError(u8"Test 1 Failed: 建实体失败");
            return 10;
        }

        const EntityID id = entity->GetEntityID();

        if (!world.Spatial().Visibility())
        {
            GLogError(u8"Test 1 Failed: 世界没有可见性存储");
            return 10;
        }

        if (!world.Spatial().IsEntityVisible(id))                         // 默认可见
        {
            GLogError(u8"Test 1 Failed: 新实体默认应可见");
            return 10;
        }

        if (world.Spatial().Visibility()->IsDirectlyInvisible(id))
        {
            GLogError(u8"Test 1 Failed: 新实体不应在不可见集合里");
            return 10;
        }

        const auto initial_revision = world.GetPrimitiveTopologyLedger().GetDynamicFactRevision(DynamicFactChange::Visibility);
        const auto initial_shadow_revision = world.GetStaticSceneRevision();
        world.Spatial().SetEntityVisible(id,false);
        if (world.Spatial().IsEntityVisible(id) || !world.Spatial().Visibility()->IsDirectlyInvisible(id))
        {
            GLogError(u8"Test 1 Failed: 置隐后查询结果不对");
            return 10;
        }

        const auto hidden_revision = world.GetPrimitiveTopologyLedger().GetDynamicFactRevision(DynamicFactChange::Visibility);
        world.Spatial().SetEntityVisible(id,false);
        if (hidden_revision != initial_revision + 1
            || world.GetPrimitiveTopologyLedger().GetDynamicFactRevision(DynamicFactChange::Visibility) != hidden_revision
            || world.GetStaticSceneRevision() != initial_shadow_revision + 1)
            return 10;

        if (world.Spatial().Visibility()->GetInvisibleCount() != 1)
        {
            GLogError(u8"Test 1 Failed: 不可见集合计数应为 1");
            return 10;
        }

        world.Spatial().SetEntityVisible(id,true);
        if (!world.Spatial().IsEntityVisible(id) || world.Spatial().Visibility()->GetInvisibleCount() != 0)
        {
            GLogError(u8"Test 1 Failed: 恢复可见后状态不对");
            return 10;
        }

        GLogInfo(u8"Test 1 Passed: 默认可见 / 置隐 / 恢复（真值唯一在世界存储）。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 祖先继承（祖先不可见 ⇒ 后代不可见，且后代自身并非"直接"不可见）
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("VisWorld2");

        auto *parent = world.CreateEntity<Entity>("VisParent");
        auto *child  = world.CreateEntity<Entity>("VisChild");
        if (!parent || !child)
        {
            GLogError(u8"Test 2 Failed: 建实体失败");
            return 11;
        }

        const EntityID parent_id = parent->GetEntityID();
        const EntityID child_id  = child->GetEntityID();

        // 建立父子关系（可见性上溯走的是变换父链 ⇒ 必须有变换行）
        const auto topology_before_transform = world.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::EntityStructure);
        const TransformID parent_transform = world.Spatial().CreateTransform(parent_id,Mobility::Static);
        const TransformID child_transform  = world.Spatial().CreateTransform(child_id,Mobility::Movable);

        if (!IsValidTransformID(parent_transform) || !IsValidTransformID(child_transform)
            || world.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::EntityStructure) != topology_before_transform + 2)
        {
            GLogError(u8"Test 2 Failed: 建变换行失败");
            return 11;
        }

        const auto parent_revision = world.GetPrimitiveTopologyLedger().GetDynamicFactRevision(DynamicFactChange::Visibility);
        world.Spatial().GetTransform(child_transform).SetParent(parent_transform);
        if (world.GetPrimitiveTopologyLedger().GetDynamicFactRevision(DynamicFactChange::Visibility) != parent_revision + 1)
            return 11;

        if (!world.Spatial().IsEntityVisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 初始应可见");
            return 11;
        }

        world.Spatial().SetEntityVisible(parent_id,false);                // 父不可见

        if (world.Spatial().IsEntityVisible(parent_id))
        {
            GLogError(u8"Test 2 Failed: 父自身置隐后仍报可见");
            return 11;
        }

        if (world.Spatial().IsEntityVisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 祖先不可见未传导到后代");
            return 11;
        }

        if (world.Spatial().Visibility()->IsDirectlyInvisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 后代被判为“直接”不可见（应为继承而来的不可见）");
            return 11;
        }

        world.Spatial().SetEntityVisible(parent_id,true);                 // 恢复父

        if (!world.Spatial().IsEntityVisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 恢复祖先后后代仍不可见");
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 祖先不可见 ⇒ 后代不可见（后代自身非“直接”不可见）。");

        const auto topology_before_destroy = world.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::EntityStructure);
        world.Spatial().DestroyTransform(child_transform);
        world.Spatial().DestroyTransform(child_transform);
        if (world.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::EntityStructure) != topology_before_destroy + 1)
        {
            GLogError(u8"Test 2 Failed: 变换销毁未正确通知拓扑变化");
            return 11;
        }
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: 销毁回收（实体没了必须从不可见集合摘掉）
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("VisWorld3");

        auto *entity = world.CreateEntity<Entity>("VisDoomed");
        if (!entity)
        {
            GLogError(u8"Test 3 Failed: 建实体失败");
            return 12;
        }

        const EntityID id = entity->GetEntityID();

        world.Spatial().SetEntityVisible(id,false);

        if (world.Spatial().Visibility()->GetInvisibleCount() != 1)
        {
            GLogError(u8"Test 3 Failed: 置隐后计数应为 1");
            return 12;
        }

        world.DestroyEntity(id);

        if (world.Spatial().Visibility()->GetInvisibleCount() != 0)
        {
            GLogError(u8"Test 3 Failed: 实体销毁后不可见标记未回收（同索引新实体会继承旧状态）");
            return 12;
        }

        auto *replacement = world.CreateEntity<Entity>("Replacement");
        if (!replacement)
            return 12;
        const EntityID replacement_id = replacement->GetEntityID();
        world.Spatial().SetEntityVisible(id, false);
        world.Spatial().SetEntityVisible(id, false);
        if (!world.Spatial().IsEntityVisible(replacement_id)
            || !EntityGPUTable::IsActive(*world.EntityRenderState().EntityGPU().Row(replacement_id))
            || !(world.EntityRenderState().EntityGPU().Row(replacement_id)->flags & uint32_t(EntityGPUFlags::Visible))
            || world.Spatial().Visibility()->GetInvisibleCount() != 0)
            return 12;

        GLogInfo(u8"Test 3 Passed: 实体销毁回收不可见标记。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: 镜像位写入纪律（值变才标脏）—— (b) 后每帧都写，不守卫就会让静态实体永远脏行
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("VisWorld4");

        auto *entity = world.CreateEntity<Entity>("VisMirror");
        if (!entity)
        {
            GLogError(u8"Test 4 Failed: 建实体失败");
            return 13;
        }

        EntityGPUTable &table = world.EntityRenderState().EntityGPU();
        const EntityID  id    = entity->GetEntityID();

        // 判据必须"隔离本次写入"：`GetDirtyCount()` 数的是**当前标脏行数** ⇒ 重标同一位不会增加
        // （行本来就脏）⇒ 用 `ClearDirty()` 先归零，再看这一次写入有没有把该行标脏。
        table.SetVisible(id,false);
        table.GetDirtySet().ClearDirty();

        table.SetVisible(id,false);                       // 同值重复写（(b) 后每帧都会发生）
        uint32_t dirty_now = table.GetDirtySet().GetDirtyCount();

        if (dirty_now != 0u)
        {
            GLogError(u8"Test 4 Failed: 同值重复写仍标脏（脏行 %u 个）⇒ 静态实体每帧整表脏",
                      dirty_now);
            return 13;
        }

        table.SetVisible(id,true);                        // 值变必须标脏
        dirty_now = table.GetDirtySet().GetDirtyCount();

        if (dirty_now == 0u)
        {
            GLogError(u8"Test 4 Failed: 值变未标脏 ⇒ GPU 侧拿不到新可见性");
            return 13;
        }

        GLogInfo(u8"Test 4 Passed: 镜像位写入值变才标脏（静态实体不产生每帧脏行）。");
    }

    GLogInfo(u8"=== All VisibilityDataStorage tests passed successfully! ===");
    return 0;
}
