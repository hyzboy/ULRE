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
}//namespace

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

    auto *entity = core_ctx.CreateEntity();
    if (!entity)
    {
        GLogError(u8"[ECSCoreBoundary] ECSCoreContext::CreateEntity 失败");
        return 3;
    }

    if (core_ctx.GetEntity(entity->GetEntityID()) != entity)
    {
        GLogError(u8"[ECSCoreBoundary] ECSCoreContext::GetEntity 寻址不匹配");
        return 4;
    }

    core_ctx.DestroyEntity(entity);
    core_ctx.Shutdown();

    GLogInfo(u8"[ECSCoreBoundary] Core 层自足性通过（无图形后端依赖）");
    return 0;
}
