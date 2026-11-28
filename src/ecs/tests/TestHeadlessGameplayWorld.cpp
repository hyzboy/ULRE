#include <hgl/ecs/core/GameplayContext.h>
#include <hgl/ecs/systems/tick/SpatialTransformSystem.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include <hgl/ecs/components/ModelCollisionProxyComponent.h>
#include <hgl/ecs/components/RigidBodyComponent.h>
#include <hgl/log/Log.h>
#include <cmath>

using namespace hgl;
using namespace hgl::ecs;

int main()
{

    ECSGameplayContext world("HeadlessGameplay");
    world.SetActive(true);

    auto tf_sys = world.RegisterSystem<SpatialTransformSystem>();
    auto physics = world.RegisterSystem<JoltPhysicsSystem>();
    if (!tf_sys || !physics)
    {
        GLogError(u8"[HeadlessGameplay] 系统注册失败");
        return 1;
    }

    // 1. 验证父子变换在无头世界中无需 Render/Vulkan 即可求值
    Entity *parent = world.CreateEntity("Parent");
    Entity *child = world.CreateEntity("Child");
    if (!parent || !child)
    {
        GLogError(u8"[HeadlessGameplay] 创建实体失败");
        return 2;
    }

    const TransformID parent_tf_id = world.Spatial().CreateTransform(parent->GetEntityID(), Mobility::Movable);
    const TransformID child_tf_id = world.Spatial().CreateTransform(child->GetEntityID(), Mobility::Movable);

    {
        TransformAccessor p = world.Spatial().GetTransform(parent_tf_id);
        TransformAccessor c = world.Spatial().GetTransform(child_tf_id);
        p.SetLocalPosition(math::Vector3f(10.0f, 0.0f, 0.0f));
        c.SetLocalPosition(math::Vector3f(0.0f, 5.0f, 0.0f));
        c.SetParent(parent_tf_id);
    }

    world.Tick(1.0f / 60.0f);

    {
        TransformAccessor c = world.Spatial().GetTransform(child_tf_id);
        const math::Vector3f wp = c.GetWorldPosition();
        if (std::fabs(wp.x - 10.0f) > 1e-4f || std::fabs(wp.y - 5.0f) > 1e-4f)
        {
            GLogError(u8"[HeadlessGameplay] 父子变换求值错误: (%.3f, %.3f, %.3f)", wp.x, wp.y, wp.z);
            return 3;
        }
    }

    // 2. 验证简化碰撞代理 + Jolt 物理步进与射线检测
    Entity *ground = world.CreateEntity("Ground");
    world.Spatial().CreateTransform(ground->GetEntityID(), Mobility::Static);
    auto *ground_proxy = ground->AddComponent<ModelCollisionProxyComponent>();
    ground_proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(20.0f, 20.0f, 1.0f));
    auto *ground_rb = ground->AddComponent<RigidBodyComponent>();
    ground_rb->motion_type = BodyMotionType::Static;

    Entity *box = world.CreateEntity("DynamicBox");
    const TransformID box_tf_id = world.Spatial().CreateTransform(box->GetEntityID(), Mobility::Movable);
    world.Spatial().GetTransform(box_tf_id).SetLocalPosition(math::Vector3f(0.0f, 0.0f, 10.0f));
    auto *box_proxy = box->AddComponent<ModelCollisionProxyComponent>();
    box_proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(0.5f, 0.5f, 0.5f));
    auto *box_rb = box->AddComponent<RigidBodyComponent>();
    box_rb->motion_type = BodyMotionType::Dynamic;
    box_rb->mass = 1.0f;

    for (int i = 0; i < 20; ++i)
        world.Tick(1.0f / 60.0f);

    const float z_after = world.Spatial().GetTransform(box_tf_id).GetWorldPosition().z;
    if (!(z_after < 10.0f))
    {
        GLogError(u8"[HeadlessGameplay] 动态刚体未在重力下下落: z=%.3f", z_after);
        return 4;
    }

    PhysicsRaycastHit hit;
    if (!physics->Raycast(math::Vector3f(0.0f, 0.0f, 5.0f), math::Vector3f(0.0f, 0.0f, -1.0f), 20.0f, hit)
        || !hit.has_hit || hit.entity != ground)
    {
        GLogError(u8"[HeadlessGameplay] 射线检测未命中地面实体");
        return 5;
    }

    // 3. 验证实体销毁自动回收空间行
    world.DestroyEntity(child);
    if (IsValidTransformID(world.Spatial().GetTransformID(child->GetEntityID())))
    {
        GLogError(u8"[HeadlessGameplay] 实体销毁后未回收空间变换行");
        return 6;
    }

    world.Shutdown();
    GLogInfo(u8"[HeadlessGameplay] 全部测试通过");
    return 0;
}
