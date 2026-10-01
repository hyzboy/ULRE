#include <hgl/ecs/support/VisibilityDataStorage.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/support/TransformAccessor.h>
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

        if (!world.GetVisibilityStorage())
        {
            GLogError(u8"Test 1 Failed: 世界没有可见性存储");
            return 10;
        }

        if (!world.IsEntityVisible(id))                         // 默认可见
        {
            GLogError(u8"Test 1 Failed: 新实体默认应可见");
            return 10;
        }

        if (world.GetVisibilityStorage()->IsDirectlyInvisible(id))
        {
            GLogError(u8"Test 1 Failed: 新实体不应在不可见集合里");
            return 10;
        }

        world.SetEntityVisible(id,false);
        if (world.IsEntityVisible(id) || !world.GetVisibilityStorage()->IsDirectlyInvisible(id))
        {
            GLogError(u8"Test 1 Failed: 置隐后查询结果不对");
            return 10;
        }

        if (world.GetVisibilityStorage()->GetInvisibleCount() != 1)
        {
            GLogError(u8"Test 1 Failed: 不可见集合计数应为 1");
            return 10;
        }

        world.SetEntityVisible(id,true);
        if (!world.IsEntityVisible(id) || world.GetVisibilityStorage()->GetInvisibleCount() != 0)
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
        const TransformID parent_transform = world.CreateTransform(parent_id,Mobility::Static);
        const TransformID child_transform  = world.CreateTransform(child_id,Mobility::Movable);

        if (!IsValidTransformID(parent_transform) || !IsValidTransformID(child_transform))
        {
            GLogError(u8"Test 2 Failed: 建变换行失败");
            return 11;
        }

        world.GetTransform(child_transform).SetParent(parent_transform);

        if (!world.IsEntityVisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 初始应可见");
            return 11;
        }

        world.SetEntityVisible(parent_id,false);                // 父不可见

        if (world.IsEntityVisible(parent_id))
        {
            GLogError(u8"Test 2 Failed: 父自身置隐后仍报可见");
            return 11;
        }

        if (world.IsEntityVisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 祖先不可见未传导到后代");
            return 11;
        }

        if (world.GetVisibilityStorage()->IsDirectlyInvisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 后代被判为“直接”不可见（应为继承而来的不可见）");
            return 11;
        }

        world.SetEntityVisible(parent_id,true);                 // 恢复父

        if (!world.IsEntityVisible(child_id))
        {
            GLogError(u8"Test 2 Failed: 恢复祖先后后代仍不可见");
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 祖先不可见 ⇒ 后代不可见（后代自身非“直接”不可见）。");
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

        world.SetEntityVisible(id,false);

        if (world.GetVisibilityStorage()->GetInvisibleCount() != 1)
        {
            GLogError(u8"Test 3 Failed: 置隐后计数应为 1");
            return 12;
        }

        world.DestroyEntity(id);

        if (world.GetVisibilityStorage()->GetInvisibleCount() != 0)
        {
            GLogError(u8"Test 3 Failed: 实体销毁后不可见标记未回收（同索引新实体会继承旧状态）");
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: 实体销毁回收不可见标记。");
    }

    GLogInfo(u8"=== All VisibilityDataStorage tests passed successfully! ===");
    return 0;
}
