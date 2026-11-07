// TestPhysicsScenarios.cpp —— 范例 1~4 场景的无头数值验证（CI 可跑，复用 PhysicsScenarios 层）

#include "PhysicsScenarios.h"

#include <hgl/ecs/components/ModelCollisionProxyComponent.h>
#include <hgl/ecs/components/RigidBodyComponent.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cmath>

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

    math::Vector3f WorldPos(ECSContext &ctx, const Entity *e)
    {
        if (!e)
            return math::Vector3f(9999.0f);
        return ctx.GetTransformByEntity(e->GetEntityID()).GetWorldPosition();
    }

    // ── 场景 1：箱体堆叠收敛 + 落箱反弹 ───────────────────────────────────────
    void TestBoxStacking()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        physics->Initialize();

        BoxStackingScenario sc = BuildBoxStackingScenario(&ctx);
        EXPECT(sc.ground != nullptr);
        EXPECT(sc.stack_boxes.size() == 6);
        EXPECT(sc.bouncy_boxes.size() == 2);

        const float dt = 0.02f;

        // 前 2 秒：堆叠不应塌散（底层仍在原位附近），反弹箱应至少一次明显向上回弹
        float bouncy_min_z[2] = {1e9f, 1e9f};
        bool    bounced[2]    = {false, false};
        float   prev_z[2]     = {WorldPos(ctx, sc.bouncy_boxes[0]).z, WorldPos(ctx, sc.bouncy_boxes[1]).z};

        StepPhysics(physics.get(), 100, dt,
                    [&](int, float)
                    {
                        for (int i = 0; i < 2; ++i)
                        {
                            float z = WorldPos(ctx, sc.bouncy_boxes[i]).z;
                            if (z < bouncy_min_z[i])
                                bouncy_min_z[i] = z;
                            // 触地（低点）后再度上升 ⇒ 反弹
                            if (!bounced[i] && bouncy_min_z[i] < 0.7f && z - prev_z[i] > 0.02f)
                                bounced[i] = true;
                            prev_z[i] = z;
                        }
                    });

        EXPECT(bounced[0]);
        EXPECT(bounced[1]);

        // 继续 4 秒让系统收敛
        StepPhysics(physics.get(), 200, dt);

        // 堆叠不穿地：所有箱体中心 z > 0
        for (Entity *b : sc.stack_boxes)
        {
            const math::Vector3f p = WorldPos(ctx, b);
            EXPECT(p.z > 0.05f);
        }

        // 顶层箱仍在金字塔附近（允许小幅滑移）
        const math::Vector3f top = WorldPos(ctx, sc.stack_boxes.back());
        EXPECT(std::abs(top.x) < 1.5f && std::abs(top.y) < 1.5f && top.z > 1.0f);

        // 反弹箱最终应settling在堆叠层附近（未穿地）
        for (Entity *b : sc.bouncy_boxes)
            EXPECT(WorldPos(ctx, b).z > 0.0f);
    }

    // ── 场景 2：胶囊/复合体沿斜坡下滑（Z-up + 局部 Z 轴胶囊）──────────────────
    void TestCapsuleCompoundSlide()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        physics->Initialize();

        CapsuleCompoundScenario sc = BuildCapsuleCompoundScenario(&ctx);
        EXPECT(sc.ground && sc.ramp && sc.capsule && sc.compound_cart);

        const math::Vector3f cap0 = WorldPos(ctx, sc.capsule);
        const math::Vector3f cart0 = WorldPos(ctx, sc.compound_cart);

        StepPhysics(physics.get(), 150, 0.02f);

        const math::Vector3f cap1 = WorldPos(ctx, sc.capsule);
        const math::Vector3f cart1 = WorldPos(ctx, sc.compound_cart);

        // 高端在 -Y 侧：下滑 ⇒ y 增大、z 减小
        EXPECT(cap1.y > cap0.y + 0.3f);
        EXPECT(cap1.z < cap0.z - 0.2f);

        // 复合小车同样应沿坡下滑（姿态可能侧翻，只查平面位移与高度）
        EXPECT(cart1.y > cart0.y + 0.2f);
        EXPECT(cart1.z < cart0.z);
    }

    // ── 场景 3：运动学平台驱动骑手与推挤障碍 ─────────────────────────────────
    void TestKinematicPlatform()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        physics->Initialize();

        KinematicPlatformScenario sc = BuildKinematicPlatformScenario(&ctx);
        EXPECT(sc.ground && sc.platform && sc.push_wall);
        EXPECT(sc.riders.size() == 3);

        const float rider_y0 = WorldPos(ctx, sc.riders[0]).y;
        float rider_y_max = -1e9f, rider_y_min = 1e9f;

        StepPhysics(physics.get(), 250, 0.02f,
                    [&](int step, float t)
                    {
                        DriveKinematicPlatform(&ctx, sc, t);
                        const float y = WorldPos(ctx, sc.riders[1]).y;
                        if (y > rider_y_max) rider_y_max = y;
                        if (y < rider_y_min) rider_y_min = y;
                    });

        // 平台本身按正弦位移过（最后一次驱动在 t = 249*0.02）
        const float last_t = 249 * 0.02f;
        const float plat_end_y = WorldPos(ctx, sc.platform).y;
        EXPECT(std::abs(plat_end_y - (sc.platform_start.y + sc.sweep_amplitude * std::sin(0.5f * last_t))) < 0.01f);

        // 骑手随平台往复：y 位移幅度显著（摩擦拖拽生效）
        EXPECT(rider_y_max - rider_y_min > 2.0f);
        // 未穿出平台范围太多
        EXPECT(rider_y_max < sc.platform_start.y + sc.sweep_amplitude + 4.0f);
        EXPECT(rider_y_min > sc.platform_start.y - sc.sweep_amplitude - 4.0f);
        (void)rider_y0;

        // 障碍箱被平台顶出接触位（推挤位移 ≈ 峰值贯穿深度 0.3）
        EXPECT(WorldPos(ctx, sc.push_wall).y > 6.3f + 0.25f);
    }

    // ── 场景 4：射线击飞 + 传感器重叠查询 ─────────────────────────────────────
    void TestRaycastSensor()
    {
        ECSContext ctx;
        auto physics = ctx.RegisterSystem<JoltPhysicsSystem>();
        physics->Initialize();

        RaycastSensorScenario sc = BuildRaycastSensorScenario(&ctx);
        EXPECT(sc.ground && sc.sensor && sc.in_sensor_box);
        EXPECT(sc.targets.size() == 4);

        // 先步进让刚体创建并落稳
        StepPhysics(physics.get(), 60, 0.02f);

        // 4.1 传感器重叠：应至少检出初始在区内的箱体
        std::vector<Entity *> in_zone;
        size_t n = QuerySensorOverlap(physics.get(), sc, in_zone);
        EXPECT(n >= 1);
        bool found_expected = false;
        for (Entity *e : in_zone)
            if (e == sc.in_sensor_box)
                found_expected = true;
        EXPECT(found_expected);

        // 4.2 射线：从空中垂直向下打 Target_B（位于原点附近），命中并施加冲量
        Entity *target = sc.targets[1];
        auto *rb = GetRigidBody(target);
        EXPECT(rb && rb->IsValidBody());
        rb->SetLinearVelocity(math::Vector3f(0.0f));

        const math::Vector3f ray_origin(WorldPos(ctx, target).x, WorldPos(ctx, target).y, 8.0f);
        Entity *hit = RaycastAndKnockback(physics.get(), ray_origin, math::Vector3f(0.0f, 0.0f, -1.0f), 12.0f,
                                          math::Vector3f(0.0f, 30.0f, 60.0f));
        EXPECT(hit == target);

        // 冲量生效：刚体获得速度
        auto *rb2 = GetRigidBody(hit);
        EXPECT(rb2 != nullptr);
        StepPhysics(physics.get(), 2, 0.01f);
        const math::Vector3f p_before = WorldPos(ctx, target);
        StepPhysics(physics.get(), 10, 0.02f);
        const math::Vector3f p_after = WorldPos(ctx, target);
        float dy = p_after.y - p_before.y;
        float dz = p_after.z - p_before.z;
        EXPECT(std::abs(dy) > 0.05f || dz > 0.05f);

        // 4.3 未命中场景：向天空发射射线不应命中
        Entity *miss = RaycastAndKnockback(physics.get(), math::Vector3f(0.0f, 0.0f, 5.0f),
                                           math::Vector3f(0.0f, 0.0f, 1.0f), 100.0f, math::Vector3f(0.0f));
        EXPECT(miss == nullptr);
    }
}//namespace

int main()
{
    logger::InitLogger(OS_TEXT("TestPhysicsScenarios"));

    TestBoxStacking();
    TestCapsuleCompoundSlide();
    TestKinematicPlatform();
    TestRaycastSensor();

    if (g_fail == 0)
    {
        GLogInfo(OS_TEXT("TestPhysicsScenarios: 全部判据通过"));
    }
    else
    {
        GLogError(OS_TEXT("TestPhysicsScenarios: %d 项判据失败"), g_fail);
    }

    return g_fail == 0 ? 0 : 1;
}
