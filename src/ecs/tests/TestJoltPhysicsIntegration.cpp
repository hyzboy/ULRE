#include <hgl/ecs/components/ModelCollisionProxyComponent.h>
#include <hgl/ecs/components/RigidBodyComponent.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>

static int g_fail = 0;

#define EXPECT(cond)                                                                \
    do                                                                              \
    {                                                                               \
        if (!(cond))                                                                \
        {                                                                           \
            GLogError(u8"判据失败: %s (%s:%d)", #cond, __FILE__, __LINE__);        \
            ++g_fail;                                                               \
        }                                                                           \
    } while (0)

namespace
{
    using namespace hgl;
    using namespace hgl::ecs;

    // ── 判据 1：RigidBodyComponent 默认属性与组件挂载 ─────────────────────────
    void TestRigidBodyDefaultsAndMount()
    {
        ECSContext ctx;
        Entity *entity = ctx.CreateEntity<Entity>("RBTestEntity");
        EXPECT(entity != nullptr);

        RigidBodyComponent *rb = entity->AddComponent<RigidBodyComponent>();
        EXPECT(rb != nullptr);
        EXPECT(rb->motion_type == BodyMotionType::Static);
        EXPECT(rb->mass == 1.0f);
        EXPECT(rb->gravity_factor == 1.0f);
        EXPECT(!rb->is_sensor);
        EXPECT(!rb->IsValidBody());

        RigidBodyComponent *queried = GetRigidBody(entity);
        EXPECT(queried == rb);
    }

    // ── 判据 2：从 ModelCollisionProxyComponent 自动创建物理 Shape（Box/Capsule/ConvexHull/复合体）──
    void TestShapeCreationFromCollisionProxy()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        EXPECT(physics != nullptr);
        physics->Initialize();

        // 2.1 盒体 Box
        {
            Entity *box_entity = ctx.CreateEntity<Entity>("BoxEntity");
            ctx.CreateTransform(box_entity->GetEntityID(), Mobility::Movable);
            auto *proxy = box_entity->AddComponent<ModelCollisionProxyComponent>();
            proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(1.0f, 2.0f, 3.0f));

            uint32_t bid = physics->CreateOrUpdateBody(box_entity);
            EXPECT(bid != 0xFFFFFFFFu);

            auto *rb = GetRigidBody(box_entity);
            EXPECT(rb != nullptr);
            EXPECT(rb->IsValidBody());
            EXPECT(rb->jolt_body_id == bid);
        }

        // 2.2 胶囊体 Capsule（Z-up 轴向映射）
        {
            Entity *capsule_entity = ctx.CreateEntity<Entity>("CapsuleEntity");
            ctx.CreateTransform(capsule_entity->GetEntityID(), Mobility::Movable);
            auto *proxy = capsule_entity->AddComponent<ModelCollisionProxyComponent>();
            proxy->AddCapsule(math::Vector3f(0.0f), 0.5f, 1.0f);

            uint32_t bid = physics->CreateOrUpdateBody(capsule_entity);
            EXPECT(bid != 0xFFFFFFFFu);

            auto *rb = GetRigidBody(capsule_entity);
            EXPECT(rb != nullptr);
            EXPECT(rb->IsValidBody());
        }

        // 2.3 凸包 ConvexHull
        {
            Entity *hull_entity = ctx.CreateEntity<Entity>("HullEntity");
            ctx.CreateTransform(hull_entity->GetEntityID(), Mobility::Movable);
            auto *proxy = hull_entity->AddComponent<ModelCollisionProxyComponent>();

            math::Vector3f vertices[8] = {
                {-1.0f, -1.0f, -1.0f},
                { 1.0f, -1.0f, -1.0f},
                { 1.0f,  1.0f, -1.0f},
                {-1.0f,  1.0f, -1.0f},
                {-1.0f, -1.0f,  1.0f},
                { 1.0f, -1.0f,  1.0f},
                { 1.0f,  1.0f,  1.0f},
                {-1.0f,  1.0f,  1.0f}
            };
            proxy->AddConvexHull(vertices, 8, math::Vector3f(0.0f));

            uint32_t bid = physics->CreateOrUpdateBody(hull_entity);
            EXPECT(bid != 0xFFFFFFFFu);

            auto *rb = GetRigidBody(hull_entity);
            EXPECT(rb != nullptr);
            EXPECT(rb->IsValidBody());
        }

        // 2.4 复合体 Compound (Box + Capsule)
        {
            Entity *compound_entity = ctx.CreateEntity<Entity>("CompoundEntity");
            ctx.CreateTransform(compound_entity->GetEntityID(), Mobility::Movable);
            auto *proxy = compound_entity->AddComponent<ModelCollisionProxyComponent>();
            proxy->AddBox(math::Vector3f(0.0f, 0.0f, 0.0f), math::Vector3f(1.0f));
            proxy->AddCapsule(math::Vector3f(0.0f, 0.0f, 2.0f), 0.3f, 0.8f);

            uint32_t bid = physics->CreateOrUpdateBody(compound_entity);
            EXPECT(bid != 0xFFFFFFFFu);

            auto *rb = GetRigidBody(compound_entity);
            EXPECT(rb != nullptr);
            EXPECT(rb->IsValidBody());
        }

        physics->Shutdown();
    }

    // ── 判据 3：动态刚体在 Z-up 重力下落与 TransformAccessor 写回同步 ────────
    void TestPhysicsDynamicFallingAndTransformSync()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        EXPECT(physics != nullptr);
        physics->Initialize();

        // 默认重力是 (0, 0, -9.81)
        math::Vector3f g = physics->GetGravity();
        EXPECT(g.z < -9.0f);

        // 创建下落的动态盒体
        Entity *dyn = ctx.CreateEntity<Entity>("FallingBox");
        ctx.CreateTransform(dyn->GetEntityID(), Mobility::Movable);
        TransformAccessor tf = ctx.GetTransformByEntity(dyn->GetEntityID());
        tf.SetWorldPosition(glm::vec3(0.0f, 0.0f, 10.0f));

        auto *proxy = dyn->AddComponent<ModelCollisionProxyComponent>();
        proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(0.5f));

        auto *rb = dyn->AddComponent<RigidBodyComponent>();
        rb->motion_type = BodyMotionType::Dynamic;
        rb->allow_sleeping = false;

        physics->CreateOrUpdateBody(dyn);
        EXPECT(rb->IsValidBody());

        const glm::vec3 start_pos = tf.GetWorldPosition();
        EXPECT(std::abs(start_pos.z - 10.0f) < 1e-3f);

        // 执行若干次物理模拟 Tick
        for (int i = 0; i < 15; ++i)
        {
            physics->Update(0.05f);
        }

        const glm::vec3 end_pos = tf.GetWorldPosition();
        GLogInfo("下落前后 Z 坐标: 初始 = %.3f, 模拟后 = %.3f", start_pos.z, end_pos.z);

        // 验证沿 -Z 轴产生了下落位移
        EXPECT(end_pos.z < start_pos.z - 0.5f);

        physics->Shutdown();
    }

    // ── 判据 4：Kinematic 刚体受 Transform 驱动更新物理位置 ─────────────────
    void TestKinematicDriving()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        EXPECT(physics != nullptr);
        physics->Initialize();

        Entity *kin = ctx.CreateEntity<Entity>("KinematicEntity");
        ctx.CreateTransform(kin->GetEntityID(), Mobility::Movable);
        TransformAccessor tf = ctx.GetTransformByEntity(kin->GetEntityID());
        tf.SetWorldPosition(glm::vec3(1.0f, 2.0f, 3.0f));

        auto *proxy = kin->AddComponent<ModelCollisionProxyComponent>();
        proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(0.5f));

        auto *rb = kin->AddComponent<RigidBodyComponent>();
        rb->motion_type = BodyMotionType::Kinematic;

        physics->CreateOrUpdateBody(kin);
        EXPECT(rb->IsValidBody());

        // 驱动 Transform 到新位置
        tf.SetWorldPosition(glm::vec3(10.0f, 20.0f, 30.0f));

        // Tick 阶段 2 会同步 MoveKinematic
        physics->Update(0.016f);

        JPH::BodyInterface *bi = physics->GetJoltBodyInterface();
        EXPECT(bi != nullptr);
        JPH::RVec3 jpos = bi->GetPosition(JPH::BodyID(rb->jolt_body_id));

        EXPECT(std::abs(jpos.GetX() - 10.0f) < 0.1f);
        EXPECT(std::abs(jpos.GetY() - 20.0f) < 0.1f);
        EXPECT(std::abs(jpos.GetZ() - 30.0f) < 0.1f);

        physics->Shutdown();
    }

    // ── 判据 5：射线检测与空间重叠查询 ──────────────────────────────────────
    void TestRaycastAndSpatialQueries()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        EXPECT(physics != nullptr);
        physics->Initialize();

        // 创建静态地面盒体，位于原点，厚度沿 Z 为 2（half_extents = (10, 10, 1)），表面在 z = 1.0f
        Entity *ground = ctx.CreateEntity<Entity>("GroundEntity");
        ctx.CreateTransform(ground->GetEntityID(), Mobility::Static);
        TransformAccessor tf = ctx.GetTransformByEntity(ground->GetEntityID());
        tf.SetWorldPosition(glm::vec3(0.0f, 0.0f, 0.0f));

        auto *proxy = ground->AddComponent<ModelCollisionProxyComponent>();
        proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(10.0f, 10.0f, 1.0f));

        auto *rb = ground->AddComponent<RigidBodyComponent>();
        rb->motion_type = BodyMotionType::Static;

        physics->CreateOrUpdateBody(ground);
        EXPECT(rb->IsValidBody());

        // 5.1 垂直向下射线检测 (0, 0, 10) -> (0, 0, -1)
        PhysicsRaycastHit hit;
        bool has_hit = physics->Raycast(math::Vector3f(0.0f, 0.0f, 10.0f), math::Vector3f(0.0f, 0.0f, -1.0f), 20.0f, hit);
        EXPECT(has_hit);
        EXPECT(hit.has_hit);
        EXPECT(hit.entity == ground);
        EXPECT(std::abs(hit.position.z - 1.0f) < 0.05f);
        // 上表面法线应指向 +Z 轴
        EXPECT(hit.normal.z > 0.9f);

        // 5.2 射线未命中测试（反方向朝上打）
        PhysicsRaycastHit miss_hit;
        bool miss = physics->Raycast(math::Vector3f(0.0f, 0.0f, 10.0f), math::Vector3f(0.0f, 0.0f, 1.0f), 20.0f, miss_hit);
        EXPECT(!miss);
        EXPECT(!miss_hit.has_hit);

        // 5.3 球体空间重叠检测 OverlapSphere
        std::vector<PhysicsOverlapHit> sphere_hits;
        physics->OverlapSphere(math::Vector3f(0.0f, 0.0f, 1.5f), 1.0f, sphere_hits);
        EXPECT(sphere_hits.size() >= 1);
        bool found_ground = false;
        for (const auto &h : sphere_hits)
        {
            if (h.entity == ground)
                found_ground = true;
        }
        EXPECT(found_ground);

        // 5.4 有向包围盒空间重叠检测 OverlapBox
        std::vector<PhysicsOverlapHit> box_hits;
        physics->OverlapBox(math::Vector3f(0.0f, 0.0f, 1.5f), math::Vector3f(2.0f), glm::identity<glm::quat>(), box_hits);
        EXPECT(box_hits.size() >= 1);
        found_ground = false;
        for (const auto &h : box_hits)
        {
            if (h.entity == ground)
                found_ground = true;
        }
        EXPECT(found_ground);

        physics->Shutdown();
    }

    // ── 判据 6：刚体释放与 OnDetach 自动清理 ────────────────────────────────
    void TestRigidBodyOnDetachCleanup()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        EXPECT(physics != nullptr);
        physics->Initialize();

        Entity *entity = ctx.CreateEntity<Entity>("CleanupTestEntity");
        ctx.CreateTransform(entity->GetEntityID(), Mobility::Movable);
        auto *proxy = entity->AddComponent<ModelCollisionProxyComponent>();
        proxy->AddBox(math::Vector3f(0.0f), math::Vector3f(1.0f));

        auto *rb = entity->AddComponent<RigidBodyComponent>();
        uint32_t bid = physics->CreateOrUpdateBody(entity);
        EXPECT(rb->IsValidBody());

        // 显式销毁
        rb->DestroyBody();
        EXPECT(!rb->IsValidBody());
        EXPECT(rb->jolt_body_id == 0xFFFFFFFFu);

        physics->Shutdown();
    }
}//namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    logger::InitLogger(OS_TEXT("TestJoltPhysicsIntegration"));

    TestRigidBodyDefaultsAndMount();
    TestShapeCreationFromCollisionProxy();
    TestPhysicsDynamicFallingAndTransformSync();
    TestKinematicDriving();
    TestRaycastAndSpatialQueries();
    TestRigidBodyOnDetachCleanup();

    if (g_fail > 0)
    {
        GLogError(u8"=== TestJoltPhysicsIntegration: %d 项判据失败 ===", g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestJoltPhysicsIntegration: 全部测试通过 ===");
    return 0;
}
