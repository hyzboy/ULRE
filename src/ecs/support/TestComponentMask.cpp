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
    class MaskProbeGeoA : public Component
    {
    public:
        MaskProbeGeoA() : Component("MaskProbeGeoA") {}
    };

    class MaskProbeTexA : public Component
    {
    public:
        MaskProbeTexA() : Component("MaskProbeTexA") {}
    };

    /// 故意不特化 ⇒ 不占槽位（None）——CPU 域组件就应该是这种
    class MaskProbeNoSlot : public Component
    {
    public:
        MaskProbeNoSlot() : Component("MaskProbeNoSlot") {}
    };

    template<> struct ComponentTypeOf<MaskProbeGeoA> { static constexpr ComponentType value = ComponentType::Geometry; };
    template<> struct ComponentTypeOf<MaskProbeTexA> { static constexpr ComponentType value = ComponentType::Texture; };
}

// 编译期自检：类型 → 槽位映射、位运算、implies（表与闭包的自检已在 ComponentTypeTable.h 里）
static_assert(ComponentTypeOf<MaskProbeGeoA>::value == ComponentType::Geometry, "槽位映射错误");
static_assert(ComponentTypeOf<MaskProbeNoSlot>::value == ComponentType::None, "未特化的类型应无槽位");
static_assert(ComponentTypeBit(ComponentType::None) == 0u, "None 不得占位");
static_assert(ComponentMaskHas(ComponentTypeBit(ComponentType::Geometry), ComponentType::Geometry), "位查询错误");
static_assert(!ComponentMaskHas(ComponentTypeBit(ComponentType::Geometry), ComponentType::Texture), "位串了");
static_assert(GetImpliedComponentMask(ComponentType::MaterialRuntime)
                  == (ComponentTypeBit(ComponentType::MaterialRuntime) | ComponentTypeBit(ComponentType::MaterialData)),
              "MaterialRuntime 必须蕴含 MaterialData（含自身位）");
static_assert(GetImpliedComponentMask(ComponentType::Transform) == ComponentTypeBit(ComponentType::Transform),
              "Transform 无蕴含，闭包应只有自身位");

/**
 * A0 地基：Entity 组件槽位位掩码（v2 §9.2 P3）
 *
 *   · 掩码是"实体有哪些槽位"的唯一表示，**单一写者 = 挂载/卸载路径**；
 *   · 与"实体实际挂载的组件"必须始终一致（`VerifyComponentMaskAgainstComponents` 对拍）；
 *   · 无槽位组件（未特化 ComponentTypeOf）不得影响掩码。
 *
 * 反证：抽掉 `Entity::ReplaceComponent` 里的 `component_mask = ComponentMaskAdd(...)` ⇒ Test 1/2 必须失败。
 */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestComponentMask"));

    GLogInfo(u8"=== Testing Entity 组件槽位位掩码 ===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: 默认无位；挂载置位；掩码与实际组件一致
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("MaskWorld1");

        auto *entity = world.CreateEntity<Entity>("MaskEntity");
        if (!entity)
        {
            GLogError(u8"Test 1 Failed: 建实体失败");
            return 10;
        }

        if (entity->GetComponentMask() != 0u)
        {
            GLogError(u8"Test 1 Failed: 新实体掩码应为 0（实际 0x%x）", entity->GetComponentMask());
            return 10;
        }

        if (!entity->VerifyComponentMaskAgainstComponents())
        {
            GLogError(u8"Test 1 Failed: 空实体掩码与实际组件不一致");
            return 10;
        }

        entity->AddComponent<MaskProbeGeoA>();

        if (!entity->HasComponentType(ComponentType::Geometry) ||
            entity->GetComponentMask() != ComponentTypeBit(ComponentType::Geometry))
        {
            GLogError(u8"Test 1 Failed: 挂载几何槽位后掩码不对（实际 0x%x）", entity->GetComponentMask());
            return 10;
        }

        if (!entity->VerifyComponentMaskAgainstComponents())
        {
            GLogError(u8"Test 1 Failed: 掩码与实际组件不一致（挂载后）");
            return 10;
        }

        // 无槽位组件不得影响掩码
        entity->AddComponent<MaskProbeNoSlot>();

        if (entity->GetComponentMask() != ComponentTypeBit(ComponentType::Geometry))
        {
            GLogError(u8"Test 1 Failed: 无槽位组件影响了掩码（实际 0x%x）", entity->GetComponentMask());
            return 10;
        }

        GLogInfo(u8"Test 1 Passed: 挂载置位 / 无槽位组件不占位 / 掩码与实际组件一致。");
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

        if (entity->GetComponentMask() != both)
        {
            GLogError(u8"Test 2 Failed: 两个槽位的掩码不对（期望 0x%x 实际 0x%x）", both, entity->GetComponentMask());
            return 11;
        }

        entity->RemoveComponent<MaskProbeGeoA>();

        if (entity->HasComponentType(ComponentType::Geometry) ||
            !entity->HasComponentType(ComponentType::Texture) ||
            entity->GetComponentMask() != ComponentTypeBit(ComponentType::Texture))
        {
            GLogError(u8"Test 2 Failed: 移除几何槽位后掩码不对（实际 0x%x）", entity->GetComponentMask());
            return 11;
        }

        if (!entity->VerifyComponentMaskAgainstComponents())
        {
            GLogError(u8"Test 2 Failed: 掩码与实际组件不一致（移除后）");
            return 11;
        }

        entity->DetachAllComponents();

        if (entity->GetComponentMask() != 0u || !entity->VerifyComponentMaskAgainstComponents())
        {
            GLogError(u8"Test 2 Failed: 全卸后掩码未归零（实际 0x%x）", entity->GetComponentMask());
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 多槽位独立 / 移除只清自己的位 / 全卸归零。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: 掩码查询是 O(1) 且不依赖组件表（哨兵：`None` 永远为假）
    // ─────────────────────────────────────────────────────────────
    {
        ECSContext world("MaskWorld3");

        auto *entity = world.CreateEntity<Entity>("MaskEntity3");
        if (!entity)
        {
            GLogError(u8"Test 3 Failed: 建实体失败");
            return 12;
        }

        if (entity->HasComponentType(ComponentType::None))
        {
            GLogError(u8"Test 3 Failed: HasComponentType(None) 必须恒假");
            return 12;
        }

        entity->AddComponent<MaskProbeGeoA>();

        if (entity->HasComponentType(ComponentType::None) ||
            entity->GetComponentMask() != ComponentTypeBit(ComponentType::Geometry))
        {
            GLogError(u8"Test 3 Failed: None 位被写进去了（掩码 0x%x）", entity->GetComponentMask());
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: None 不占位，掩码只含真实槽位。");
    }

    GLogInfo(u8"=== All ComponentMask tests passed successfully! ===");
    return 0;
}
