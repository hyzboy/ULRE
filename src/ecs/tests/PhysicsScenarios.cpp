// PhysicsScenarios.cpp —— 范例 1~4 纯 ECS 场景搭建实现（见 PhysicsScenarios.h 头注释）

#include "PhysicsScenarios.h"

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/components/ModelCollisionProxyComponent.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>

#include <cmath>

namespace hgl::ecs
{
    namespace
    {
        // 公共实体工厂：建实体 + 变换（世界位姿直写）+ 代理组件挂载钩子 + 刚体参数
        template <typename FillProxyFn>
        Entity *CreateBodyEntity(ECSContext *ctx,
                                 const char *name,
                                 const math::Vector3f &pos,
                                 const glm::quat &rot,
                                 const PhysicsBodyDesc &desc,
                                 FillProxyFn fill_proxy)
        {
            Entity *entity = ctx->CreateEntity<Entity>(name);
            if (!entity)
                return nullptr;

            const Mobility mobility = (desc.motion == BodyMotionType::Static)
                                          ? Mobility::Static
                                          : Mobility::Movable;
            ctx->CreateTransform(entity->GetEntityID(), mobility);

            TransformAccessor transform = ctx->GetTransformByEntity(entity->GetEntityID());
            transform.SetWorldPosition(pos);
            transform.SetWorldRotation(rot);

            auto *proxy = entity->AddComponent<ModelCollisionProxyComponent>();
            fill_proxy(*proxy);

            auto *rb = entity->AddComponent<RigidBodyComponent>();
            rb->motion_type      = desc.motion;
            rb->mass             = desc.mass;
            rb->friction         = desc.friction;
            rb->restitution      = desc.restitution;
            rb->linear_damping   = desc.linear_damping;
            rb->angular_damping  = desc.angular_damping;
            rb->gravity_factor   = desc.gravity_factor;
            rb->is_sensor        = desc.is_sensor;
            rb->allow_sleeping   = desc.allow_sleeping;

            return entity;
        }
    }//namespace

    Entity *CreatePhysicsBox(ECSContext *ctx,
                             const char *name,
                             const math::Vector3f &pos,
                             const glm::quat &rot,
                             const math::Vector3f &half_extents,
                             const PhysicsBodyDesc &desc)
    {
        return CreateBodyEntity(ctx, name, pos, rot, desc,
                                [&](ModelCollisionProxyComponent &proxy)
                                { proxy.AddBox(math::Vector3f(0.0f), half_extents); });
    }

    Entity *CreatePhysicsCapsule(ECSContext *ctx,
                                 const char *name,
                                 const math::Vector3f &pos,
                                 const glm::quat &rot,
                                 float radius,
                                 float half_height,
                                 const PhysicsBodyDesc &desc)
    {
        return CreateBodyEntity(ctx, name, pos, rot, desc,
                                [&](ModelCollisionProxyComponent &proxy)
                                { proxy.AddCapsule(math::Vector3f(0.0f), radius, half_height); });
    }

    Entity *CreatePhysicsCompoundBoxEntity(ECSContext *ctx,
                                           const char *name,
                                           const math::Vector3f &pos,
                                           const glm::quat &rot,
                                           const std::vector<math::Vector3f> &local_centers,
                                           const std::vector<math::Vector3f> &half_extents,
                                           const PhysicsBodyDesc &desc)
    {
        return CreateBodyEntity(ctx, name, pos, rot, desc,
                                [&](ModelCollisionProxyComponent &proxy)
                                {
                                    const size_t count = local_centers.size() < half_extents.size()
                                                             ? local_centers.size()
                                                             : half_extents.size();
                                    for (size_t i = 0; i < count; ++i)
                                        proxy.AddBox(local_centers[i], half_extents[i]);
                                });
    }

    // ── 范例 1：箱体堆叠与弹性反弹 ────────────────────────────────────────────

    BoxStackingScenario BuildBoxStackingScenario(ECSContext *ctx)
    {
        BoxStackingScenario sc;

        {
            PhysicsBodyDesc g;
            g.motion   = BodyMotionType::Static;
            g.friction = 0.6f;
            sc.ground = CreatePhysicsBox(ctx, "BoxStack_Ground",
                                         math::Vector3f(0.0f, 0.0f, -0.5f),
                                         glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                         math::Vector3f(20.0f, 20.0f, 0.5f), g);
        }

        // 3-2-1 金字塔（箱体半长 0.5 ⇒ 1m 立方；层中心 z = 0.5 / 1.5 / 2.5）
        {
            PhysicsBodyDesc b;
            b.motion = BodyMotionType::Dynamic;
            b.friction = 0.6f;
            b.restitution = 0.0f;
            b.mass = 1.0f;

            const math::Vector3f layer0[3] = {{-1.0f, 0.0f, 0.5f}, {0.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 0.5f}};
            const math::Vector3f layer1[2] = {{-0.5f, 0.0f, 1.5f}, {0.5f, 0.0f, 1.5f}};
            const math::Vector3f layer2[1] = {{0.0f, 0.0f, 2.5f}};

            for (const auto &p : layer0)
                sc.stack_boxes.push_back(CreatePhysicsBox(ctx, "Stack_L0", p, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), math::Vector3f(0.5f), b));
            for (const auto &p : layer1)
                sc.stack_boxes.push_back(CreatePhysicsBox(ctx, "Stack_L1", p, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), math::Vector3f(0.5f), b));
            for (const auto &p : layer2)
                sc.stack_boxes.push_back(CreatePhysicsBox(ctx, "Stack_L2", p, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), math::Vector3f(0.5f), b));
        }

        // 高弹性落箱：从高处下落砸向地面（避开金字塔，x 侧位），演示 restitution 反弹
        {
            PhysicsBodyDesc e;
            e.motion = BodyMotionType::Dynamic;
            e.friction = 0.4f;
            e.restitution = 0.8f;
            e.mass = 2.0f;
            e.allow_sleeping = false;
            e.linear_damping = 0.01f;

            sc.bouncy_boxes.push_back(CreatePhysicsBox(ctx, "Bouncy_A",
                                                       math::Vector3f(4.0f, 0.0f, 10.0f),
                                                       glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                       math::Vector3f(0.5f), e));
            sc.bouncy_boxes.push_back(CreatePhysicsBox(ctx, "Bouncy_B",
                                                       math::Vector3f(-4.0f, 2.0f, 14.0f),
                                                       glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                                       math::Vector3f(0.5f), e));
        }

        return sc;
    }

    // ── 范例 2：Z-up 胶囊体与复合形状斜坡滑滚 ────────────────────────────────

    CapsuleCompoundScenario BuildCapsuleCompoundScenario(ECSContext *ctx)
    {
        CapsuleCompoundScenario sc;
        const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);

        // 斜坡：绕 X 轴 -20°（局部 +Y 指向「下坡」方向，高端在 -Y 侧）
        const float ramp_angle = -20.0f * 3.14159265358979f / 180.0f;
        const glm::quat ramp_rot = glm::angleAxis(ramp_angle, glm::vec3(1.0f, 0.0f, 0.0f));
        // 斜坡上表面参考系：P(t) = C + n*half_z + t*ŷ'（t∈[-4,4] 沿板面）
        const float ca = std::cos(ramp_angle), sa = std::sin(ramp_angle);
        const math::Vector3f ramp_center(0.0f, 3.0f, 1.5f);
        const math::Vector3f ramp_half(2.0f, 4.0f, 0.25f);
        // 板面法线（局部 Z 的世界像）：(0, -sinθ, cosθ)；下滑方向（局部 +Y）：(0, cosθ, sinθ)
        const math::Vector3f n(0.0f, -sa, ca);
        const math::Vector3f y_axis(0.0f, ca, sa);

        {
            PhysicsBodyDesc g;
            g.motion = BodyMotionType::Static;
            g.friction = 0.4f;
            sc.ground = CreatePhysicsBox(ctx, "Ramp_Ground",
                                         math::Vector3f(0.0f, 0.0f, -0.5f), identity,
                                         math::Vector3f(20.0f, 20.0f, 0.5f), g);
        }

        {
            PhysicsBodyDesc r;
            r.motion = BodyMotionType::Static;
            r.friction = 0.15f;
            sc.ramp = CreatePhysicsBox(ctx, "Ramp", ramp_center, ramp_rot, ramp_half, r);
        }

        // 胶囊：半径 0.4 / 半高 0.8，轴沿局部 Z ⇒ 实体姿态与斜坡同倾角（Z 轴贴板法线）
        {
            const float t_on_ramp = -2.5f;
            math::Vector3f surface = ramp_center + n * ramp_half.z + y_axis * t_on_ramp;
            math::Vector3f center = surface + n * (0.8f + 0.4f);
            PhysicsBodyDesc c;
            c.motion = BodyMotionType::Dynamic;
            c.mass = 3.0f;
            c.friction = 0.2f;
            c.restitution = 0.1f;
            c.angular_damping = 0.01f;
            sc.capsule = CreatePhysicsCapsule(ctx, "Ramp_Capsule", center, ramp_rot, 0.4f, 0.8f, c);
        }

        // 复合「小车」：底盘箱 + 左右轮箱（局部空间组合体），同样贴坡放置
        {
            const float t_on_ramp = -1.2f;
            math::Vector3f surface = ramp_center + n * ramp_half.z + y_axis * t_on_ramp;
            math::Vector3f center = surface + n * 0.8f;
            PhysicsBodyDesc c;
            c.motion = BodyMotionType::Dynamic;
            c.mass = 4.0f;
            c.friction = 0.3f;
            c.angular_damping = 0.02f;
            sc.compound_cart = CreatePhysicsCompoundBoxEntity(
                ctx, "Ramp_Cart", center, ramp_rot,
                { math::Vector3f(0.0f, 0.0f, 0.5f),
                  math::Vector3f(-0.8f, 0.0f, 0.0f),
                  math::Vector3f(0.8f, 0.0f, 0.0f) },
                { math::Vector3f(0.7f, 1.1f, 0.3f),
                  math::Vector3f(0.15f, 0.5f, 0.5f),
                  math::Vector3f(0.15f, 0.5f, 0.5f) },
                c);
        }

        return sc;
    }

    // ── 范例 3：运动学平台推挤 ───────────────────────────────────────────────

    KinematicPlatformScenario BuildKinematicPlatformScenario(ECSContext *ctx)
    {
        KinematicPlatformScenario sc;
        const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);

        {
            PhysicsBodyDesc g;
            g.motion = BodyMotionType::Static;
            g.friction = 0.6f;
            sc.ground = CreatePhysicsBox(ctx, "Platform_Ground",
                                         math::Vector3f(0.0f, 0.0f, -0.5f), identity,
                                         math::Vector3f(24.0f, 24.0f, 0.5f), g);
        }

        {
            PhysicsBodyDesc p;
            p.motion = BodyMotionType::Kinematic;
            p.friction = 0.9f;
            sc.platform = CreatePhysicsBox(ctx, "Kinematic_Platform",
                                           sc.platform_start, identity,
                                           math::Vector3f(2.0f, 2.0f, 0.25f), p);
        }

        // 平台顶面 z = 1.0：三枚骑手箱体中心 z = 1.5
        {
            PhysicsBodyDesc r;
            r.motion = BodyMotionType::Dynamic;
            r.mass = 1.0f;
            r.friction = 0.8f;
            const math::Vector3f spots[3] = {{-1.0f, 0.0f, 1.5f}, {0.0f, 0.0f, 1.5f}, {1.0f, 0.5f, 1.5f}};
            for (const auto &s : spots)
                sc.riders.push_back(CreatePhysicsBox(ctx, "Platform_Rider", s, identity, math::Vector3f(0.5f), r));
        }

        // 平台行程远端（+Y，最大 4.0，板缘 6.0）前方一枚动态障碍箱：被平台推挤滑行
        {
            PhysicsBodyDesc o;
            o.motion = BodyMotionType::Dynamic;
            o.mass = 2.0f;
            o.friction = 0.4f;
            sc.push_wall = CreatePhysicsBox(ctx, "Platform_Obstacle",
                                                math::Vector3f(0.0f, 6.3f, 0.5f), identity,
                                            math::Vector3f(2.0f, 0.6f, 0.5f), o);
        }

        return sc;
    }

    void DriveKinematicPlatform(ECSContext *ctx, const KinematicPlatformScenario &sc, float time_seconds)
    {
        if (!ctx || !sc.platform)
            return;

        TransformAccessor transform = ctx->GetTransformByEntity(sc.platform->GetEntityID());
        if (!transform.IsValid())
            return;

        // 沿 Y 正弦往复：y(t) = y0 + A·sin(0.5·t)
        const float y = sc.platform_start.y + sc.sweep_amplitude * std::sin(0.5f * time_seconds);
        transform.SetWorldPosition(math::Vector3f(sc.platform_start.x, y, sc.platform_start.z));
    }

    // ── 范例 4：射线击飞与传感器区域 ─────────────────────────────────────────

    RaycastSensorScenario BuildRaycastSensorScenario(ECSContext *ctx)
    {
        RaycastSensorScenario sc;
        const glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);

        {
            PhysicsBodyDesc g;
            g.motion = BodyMotionType::Static;
            g.friction = 0.5f;
            sc.ground = CreatePhysicsBox(ctx, "Sensor_Ground",
                                         math::Vector3f(0.0f, 0.0f, -0.5f), identity,
                                         math::Vector3f(20.0f, 20.0f, 0.5f), g);
        }

        // 传感器体积：只产生重叠、不阻挡（is_sensor ⇒ SENSOR 层 + mIsSensor）
        {
            PhysicsBodyDesc s;
            s.motion = BodyMotionType::Static;
            s.is_sensor = true;
            s.gravity_factor = 0.0f;
            sc.sensor = CreatePhysicsBox(ctx, "Sensor_Zone", sc.sensor_center, identity, sc.sensor_half, s);
        }

        // 三枚散落靶箱 + 一枚初始位于传感器区内（从略高处落下，落定在区内）
        {
            PhysicsBodyDesc t;
            t.motion = BodyMotionType::Dynamic;
            t.mass = 1.0f;
            t.friction = 0.5f;
            t.allow_sleeping = false;

            sc.targets.push_back(CreatePhysicsBox(ctx, "Target_A", math::Vector3f(0.0f, -3.0f, 0.5f), identity, math::Vector3f(0.5f), t));
            sc.targets.push_back(CreatePhysicsBox(ctx, "Target_B", math::Vector3f(0.0f, 0.0f, 0.5f), identity, math::Vector3f(0.5f), t));
            sc.targets.push_back(CreatePhysicsBox(ctx, "Target_C", math::Vector3f(0.0f, 3.0f, 0.5f), identity, math::Vector3f(0.5f), t));
            sc.in_sensor_box = CreatePhysicsBox(ctx, "Target_Sensor", math::Vector3f(sc.sensor_center.x, sc.sensor_center.y, sc.sensor_center.z), identity, math::Vector3f(0.5f), t);
            sc.targets.push_back(sc.in_sensor_box);
        }

        return sc;
    }

    Entity *RaycastAndKnockback(JoltPhysicsSystem *physics,
                                const math::Vector3f &origin,
                                const math::Vector3f &direction,
                                float max_distance,
                                const math::Vector3f &impulse)
    {
        if (!physics)
            return nullptr;

        PhysicsRaycastHit hit;
        if (!physics->Raycast(origin, direction, max_distance, hit) || !hit.entity)
            return nullptr;

        auto *rb = GetRigidBody(hit.entity);
        if (rb && rb->IsValidBody())
        {
            rb->Activate();
            rb->AddImpulse(impulse);
        }

        return hit.entity;
    }

    size_t QuerySensorOverlap(JoltPhysicsSystem *physics,
                              const RaycastSensorScenario &sc,
                              std::vector<Entity *> &out_entities)
    {
        out_entities.clear();
        if (!physics)
            return 0;

        std::vector<PhysicsOverlapHit> hits;
        physics->OverlapBox(sc.sensor_center, sc.sensor_half, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), hits);

        for (const auto &h : hits)
        {
            if (!h.entity)
                continue;

            auto *rb = GetRigidBody(h.entity);
            // 传感器查询只统计「动态实体进入体积」，排除地面/传感器自身
            if (!rb || rb->motion_type != BodyMotionType::Dynamic || rb->is_sensor)
                continue;

            out_entities.push_back(h.entity);
        }

        return out_entities.size();
    }

    // ── 步进工具 ─────────────────────────────────────────────────────────────

    void StepPhysics(JoltPhysicsSystem *physics, int steps, float step_seconds,
                     const std::function<void(int step_index, float time)> &extra_per_step)
    {
        if (!physics)
            return;

        float time = 0.0f;
        for (int i = 0; i < steps; ++i)
        {
            if (extra_per_step)
                extra_per_step(i, time);
            physics->Update(step_seconds);
            time += step_seconds;
        }
    }
}//namespace hgl::ecs
