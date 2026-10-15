#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/support/ComponentTypeTable.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

namespace hgl::ecs
{
    // ── 测试用假组件（占不同槽位；特化写在"类型自己"这一侧）────────────────
    // T2-c：组件住世界的类型化对象池，行头是 `ComponentRow`（宿主元数据），不再有 `Component` 基类。
    class MaskProbeGeoA : public ComponentRow
    {
    public:
        MaskProbeGeoA() = default;
    };

    class MaskProbeTexA : public ComponentRow
    {
    public:
        MaskProbeTexA() = default;
    };

    /// 故意不特化 ⇒ **没有类型号**（None）。
    /// T2-c 起这种类型**不能挂到实体上**：实体槽 = 按类型行号，没有号就没有槽 ⇒
    /// `entity->AddComponent<MaskProbeNoSlot>()` 是**编译期错误**（不再是运行期静默不占位）。
    class MaskProbeNoSlot : public ComponentRow
    {
    public:
        MaskProbeNoSlot() = default;
    };

    template<> struct ComponentTypeOf<MaskProbeGeoA> { static constexpr ComponentType value = ComponentType::Geometry; };
    template<> struct ComponentTypeOf<MaskProbeTexA> { static constexpr ComponentType value = ComponentType::Texture; };
}

// 编译期自检：类型 → 槽位映射、位运算、implies（表与闭包的自检已在 ComponentTypeTable.h 里）
static_assert(ComponentTypeOf<MaskProbeGeoA>::value == ComponentType::Geometry, "槽位映射错误");
static_assert(ComponentTypeOf<MaskProbeNoSlot>::value == ComponentType::None, "未特化的类型应无类型号");
static_assert(ComponentTypeBit(ComponentType::None) == 0u, "None 不得占位");
static_assert(ComponentMaskHas(ComponentTypeBit(ComponentType::Geometry), ComponentType::Geometry), "位查询错误");
static_assert(!ComponentMaskHas(ComponentTypeBit(ComponentType::Geometry), ComponentType::Texture), "位串了");
static_assert(GetImpliedComponentMask(ComponentType::MaterialRuntime)
                  == (ComponentTypeBit(ComponentType::MaterialRuntime) | ComponentTypeBit(ComponentType::MaterialData)),
              "MaterialRuntime 必须蕴含 MaterialData（含自身位）");
static_assert(GetImpliedComponentMask(ComponentType::Transform) == ComponentTypeBit(ComponentType::Transform),
              "Transform 无蕴含，闭包应只有自身位");

/**
 * A0 地基 + T2-c + **A1(b)（2026-10-14）**：Entity 组件槽位
 *
 *   · **唯一真源 = `component_rows[ComponentType]`**（T2-c：按类型行号，0 = 无）；
 *   · **掩码不再是字段**（A1(b)）：策略路径需要位图时由 `Entity::BuildComponentMask()` **现场拼**
 *     ⇒ 同一事实不存两份（原先的"缓存视图 + 一致性对拍器"随字段一并删除）；
 *   · 「有没有挂某槽位」问 `HasComponent<T>()` 或 `GetComponentRow(type) != 0` —— 两条都不经掩码；
 *   · **没有类型号的类型不能挂**（未特化 `ComponentTypeOf` ⇒ `None`）—— 这是**编译期**判据
 *     （`EnsureTypedPool<T>` 的 static_assert），不再是运行期"静默不占位"。
 *
 * 反证：让 `Entity::BuildComponentMask()` 漏掉一个槽位（如循环起点从 2 起）⇒ Test 1/2 必须失败。
 */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestComponentRows"));

    GLogInfo(u8"=== Testing Entity 组件槽位（行表唯一真源 / 掩码现场拼）===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: 默认无槽位；挂载后行表置位、派生掩码同步
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("MaskWorld1");

        auto *entity = world.CreateEntity<Entity>("MaskEntity");
        if (!entity)
        {
            GLogError(u8"Test 1 Failed: 建实体失败");
            return 10;
        }

        if (entity->BuildComponentMask() != 0u)
        {
            GLogError(u8"Test 1 Failed: 新实体派生掩码应为 0（实际 0x%x）", entity->BuildComponentMask());
            return 10;
        }

        entity->AddComponent<MaskProbeGeoA>();

        if (entity->GetComponentRow(ComponentType::Geometry) == 0 ||
            entity->BuildComponentMask() != ComponentTypeBit(ComponentType::Geometry))
        {
            GLogError(u8"Test 1 Failed: 挂载几何槽位后行表/派生掩码不对（row=%u mask=0x%x）",
                      entity->GetComponentRow(ComponentType::Geometry),
                      entity->BuildComponentMask());
            return 10;
        }

        if (!entity->HasComponent<MaskProbeGeoA>())
        {
            GLogError(u8"Test 1 Failed: HasComponent<T>() 未反映挂载");
            return 10;
        }

        // T2-c：无类型号的类型**挂不上去**（编译期被 `EnsureTypedPool<T>` 的 static_assert 拦住）
        // ⇒ 掩码不可能被它污染。运行期判据 = 该类型在类型表/特化里确实是 None。
        if (ComponentTypeOf<MaskProbeNoSlot>::value != ComponentType::None)
        {
            GLogError(u8"Test 1 Failed: 未特化的组件类型应无类型号");
            return 10;
        }

        GLogInfo(u8"Test 1 Passed: 行表置位 / 派生掩码同步 / 无类型号组件编译期被拒。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 多槽位彼此独立；移除只清自己的位；全卸归零
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("MaskWorld2");

        auto *entity = world.CreateEntity<Entity>("MaskEntity2");
        if (!entity)
        {
            GLogError(u8"Test 2 Failed: 建实体失败");
            return 11;
        }

        entity->AddComponent<MaskProbeGeoA>();
        entity->AddComponent<MaskProbeTexA>();

        const uint32_t both = ComponentTypeBit(ComponentType::Geometry) | ComponentTypeBit(ComponentType::Texture);

        if (entity->BuildComponentMask() != both)
        {
            GLogError(u8"Test 2 Failed: 两个槽位的派生掩码不对（期望 0x%x 实际 0x%x）",
                      both, entity->BuildComponentMask());
            return 11;
        }

        entity->RemoveComponent<MaskProbeGeoA>();

        if (entity->GetComponentRow(ComponentType::Geometry) != 0 ||
            entity->GetComponentRow(ComponentType::Texture) == 0 ||
            entity->BuildComponentMask() != ComponentTypeBit(ComponentType::Texture))
        {
            GLogError(u8"Test 2 Failed: 移除几何槽位后行表/派生掩码不对（row=%u mask=0x%x）",
                      entity->GetComponentRow(ComponentType::Geometry),
                      entity->BuildComponentMask());
            return 11;
        }

        entity->DetachAllComponents();

        if (entity->BuildComponentMask() != 0u)
        {
            GLogError(u8"Test 2 Failed: 全卸后派生掩码未归零（实际 0x%x）", entity->BuildComponentMask());
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 多槽位独立 / 移除只清自己 / 全卸归零。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: `None` 不占位（哨兵：行号与掩码都不得含 None）
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("MaskWorld3");

        auto *entity = world.CreateEntity<Entity>("MaskEntity3");
        if (!entity)
        {
            GLogError(u8"Test 3 Failed: 建实体失败");
            return 12;
        }

        if (entity->GetComponentRow(ComponentType::None) != 0)
        {
            GLogError(u8"Test 3 Failed: None 的行号必须恒 0");
            return 12;
        }

        entity->AddComponent<MaskProbeGeoA>();

        if (entity->GetComponentRow(ComponentType::None) != 0 ||
            entity->BuildComponentMask() != ComponentTypeBit(ComponentType::Geometry))
        {
            GLogError(u8"Test 3 Failed: None 位被写进去了（掩码 0x%x）", entity->BuildComponentMask());
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: None 不占位，派生掩码只含真实槽位。");
    }

    GLogInfo(u8"=== All ComponentMask tests passed successfully! ===");
    return 0;
}
