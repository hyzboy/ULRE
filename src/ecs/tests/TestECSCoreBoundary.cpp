/**
 * ECS.Core 边界闸门。
 *
 * 目的：把「Core 不依赖图形后端」从注释约定变成**可构建的证明**。
 * 本 TU 只包含 Core 层头文件、只链接 ULRE.ECS.Core：
 *   · 任何 Core 头重新引入 hgl/graph、hgl/vk 或 vulkan 头，都会在这里编不过；
 *   · 任何 Core 实现把符号落到 Render 侧，都会在链接本可执行时失败。
 *
 * 新增文件请按分层口径归类（见 src/ecs/CMakeLists.txt 的清单说明），
 * 并把 Core 侧的主要头补进下面的 include 列表。
 */

#include<hgl/ecs/core/entity/Object.h>
#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/ecs/core/entity/EntityHandle.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/ecs/core/entity/EntityManager.h>
#include<hgl/ecs/core/ECSCoreContext.h>
#include<hgl/ecs/core/system/System.h>
#include<hgl/ecs/core/system/SystemGroup.h>
#include<hgl/ecs/core/system/SystemOrderPlanner.h>
#include<hgl/ecs/core/system/SystemRegistry.h>
#include<hgl/ecs/support/storage/ComponentTypeTable.h>
#include<hgl/ecs/support/transform/TransformID.h>
#include<hgl/ecs/support/cull/BoundingVolumeCull.h>

#include<hgl/log/Log.h>

namespace
{
    class BoundaryProbeObject : public hgl::ecs::Object
    {
    public:

        explicit BoundaryProbeObject(const char *name) : Object(name) {}
    };

    struct BoundaryTestComponent : public hgl::ecs::ComponentRow
    {
        int value = 42;
        int *detach_count = nullptr;
        bool *owner_alive = nullptr;

        BoundaryTestComponent() = default;
        BoundaryTestComponent(int *count, bool *alive) : detach_count(count), owner_alive(alive) {}

        void OnDetach()
        {
            if (detach_count)
                ++*detach_count;
            if (owner_alive)
                *owner_alive = owner && owner->GetCoreContext()
                             && owner->GetCoreContext()->GetEntity(owner->GetEntityID()) == owner;
        }
    };

    class BoundaryTestSystem : public hgl::ecs::System
    {
    public:
        int tick_count = 0;

        void Update(float dt) override
        {
            ++tick_count;
        }
    };
}//namespace

namespace hgl::ecs
{
    template<>
    struct ComponentTypeOf<BoundaryTestComponent>
    {
        static constexpr ComponentType value = ComponentType::Lines;
    };
}

int main()
{
    BoundaryProbeObject probe("ECSCoreBoundary");

    if (probe.GetName() != "ECSCoreBoundary")
    {
        GLogError(u8"[ECSCoreBoundary] Object 名称契约失效");
        return 1;
    }

    hgl::ecs::SystemGroup group;
    group.name = "BoundaryProbe";

    if (group.name != "BoundaryProbe")
    {
        GLogError(u8"[ECSCoreBoundary] SystemGroup 可构造性契约失效");
        return 2;
    }

    // 验证 ECSCoreContext 纯核心上下文运作（零图形依赖）
    hgl::ecs::ECSCoreContext core_ctx("BoundaryCoreWorld");
    core_ctx.SetActive(true);

    // 1. 系统注册与帧驱动验证
    auto sys = core_ctx.RegisterSystem<BoundaryTestSystem>();
    if (!sys || core_ctx.GetSystem<BoundaryTestSystem>() != sys)
    {
        GLogError(u8"[ECSCoreBoundary] RegisterSystem / GetSystem 契约失效");
        return 3;
    }

    core_ctx.Tick(0.016f);
    if (sys->tick_count != 1)
    {
        GLogError(u8"[ECSCoreBoundary] Tick 系统派发契约失效");
        return 4;
    }

    // 2. 实体分配与寻址验证
    auto *entity = core_ctx.CreateEntity();
    if (!entity)
    {
        GLogError(u8"[ECSCoreBoundary] ECSCoreContext::CreateEntity 失败");
        return 5;
    }

    if (core_ctx.GetEntity(entity->GetEntityID()) != entity)
    {
        GLogError(u8"[ECSCoreBoundary] ECSCoreContext::GetEntity 寻址不匹配");
        return 6;
    }

    // 3. 组件池与组件挂载验证
    auto *comp = entity->AddComponent<BoundaryTestComponent>();
    if (!comp || comp->value != 42)
    {
        GLogError(u8"[ECSCoreBoundary] AddComponent 契约失效");
        return 7;
    }

    const uint32_t comp_row = entity->GetComponentRow(hgl::ecs::ComponentType::Lines);
    if (comp_row == 0 || core_ctx.GetComponentByRow<BoundaryTestComponent>(comp_row) != comp)
    {
        GLogError(u8"[ECSCoreBoundary] GetComponentByRow 寻址不匹配");
        return 8;
    }

    // 4. 组件销毁与实体销毁
    entity->RemoveComponent<BoundaryTestComponent>();
    if (entity->GetComponentRow(hgl::ecs::ComponentType::Lines) != 0)
    {
        GLogError(u8"[ECSCoreBoundary] RemoveComponent 契约失效");
        return 9;
    }

    core_ctx.DestroyEntity(entity);
    core_ctx.Shutdown();

    int detach_count = 0;
    bool owner_alive = false;
    hgl::ecs::ECSCoreContext lifecycle("LifecycleBoundary");
    auto *first = lifecycle.CreateEntity();
    if (!first || !first->AddComponent<BoundaryTestComponent>(&detach_count, &owner_alive)
        || !first->AddComponent<BoundaryTestComponent>(&detach_count, &owner_alive)
        || detach_count != 1 || !owner_alive)
    {
        GLogError(u8"[ECSCoreBoundary] 组件替换时卸载次数或宿主生命周期不正确");
        return 10;
    }

    const hgl::ecs::EntityID old_id = first->GetEntityID();
    lifecycle.DestroyEntity(old_id);
    auto *second = lifecycle.CreateEntity();
    if (detach_count != 2 || !owner_alive || !second || lifecycle.GetEntity(old_id))
    {
        GLogError(u8"[ECSCoreBoundary] 实体销毁或旧句柄失效契约不正确");
        return 11;
    }

    if (!second->AddComponent<BoundaryTestComponent>(&detach_count, &owner_alive))
        return 12;

    lifecycle.Shutdown();
    lifecycle.Shutdown();
    if (detach_count != 3 || !owner_alive)
    {
        GLogError(u8"[ECSCoreBoundary] 重复关闭导致组件重复卸载或宿主提前销毁");
        return 13;
    }

    GLogInfo(u8"[ECSCoreBoundary] Core 层自足性与解耦闸门全部通过（零图形依赖）");
    return 0;
}
