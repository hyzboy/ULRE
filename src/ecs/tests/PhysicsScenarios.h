#pragma once
// PhysicsScenarios.h —— JoltPhysics 范例 1~4 的「纯 ECS 场景搭建」层（无渲染依赖）
//
// 设计约束：
//   · 只依赖 ECSContext / Entity / TransformAccessor / ModelCollisionProxyComponent /
//     RigidBodyComponent / JoltPhysicsSystem，不引用任何 Vulkan / 图元 / 材质类型；
//   · 同一份场景函数同时服务两侧：
//     - `src/ecs/tests/TestPhysicsScenarios.cpp`：无头步进 + 数值断言（CI 可跑）；
//     - `example/Physics/*`：GUI 范例在返回实体上挂渲染组件。
//   · 所有场景把「物理结果」写进返回结构体的实体指针列表，调用方用
//     TransformAccessor 读取即可验证。

#include <hgl/ecs/components/RigidBodyComponent.h>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <functional>
#include <cstddef>

namespace hgl::ecs
{
    class ECSContext;
    class Entity;
    class JoltPhysicsSystem;

    // ── 通用体工厂（Box / Capsule / Compound 的「ECS 实体」形态封装）──────────

    /// 刚体构建参数（对应 RigidBodyComponent 的可调属性）
    struct PhysicsBodyDesc
    {
        BodyMotionType motion        = BodyMotionType::Dynamic;
        float          mass          = 1.0f;
        float          friction      = 0.3f;
        float          restitution   = 0.0f;
        float          linear_damping  = 0.05f;
        float          angular_damping = 0.05f;
        float          gravity_factor  = 1.0f;
        bool           is_sensor       = false;
        bool           allow_sleeping  = true;
    };

    /**
     * @brief 创建一个「世界位置 pos + 世界旋转 rot」的 Box 刚体实体。
     *
     * `half_extents` 为盒体半长（局部空间）；实体同时挂载
     * ModelCollisionProxyComponent（1 个 Box body）与 RigidBodyComponent。
     */
    Entity *CreatePhysicsBox(ECSContext *ctx,
                             const char *name,
                             const math::Vector3f &pos,
                             const glm::quat &rot,
                             const math::Vector3f &half_extents,
                             const PhysicsBodyDesc &desc);

    /// 创建一个 Capsule 刚体实体（轴 = 局部 Z，符合引擎 Z-up 约定）
    Entity *CreatePhysicsCapsule(ECSContext *ctx,
                                 const char *name,
                                 const math::Vector3f &pos,
                                 const glm::quat &rot,
                                 float radius,
                                 float half_height,
                                 const PhysicsBodyDesc &desc);

    /// 创建一个复合形状（多 Box 组合）刚体实体
    Entity *CreatePhysicsCompoundBoxEntity(ECSContext *ctx,
                                           const char *name,
                                           const math::Vector3f &pos,
                                           const glm::quat &rot,
                                           const std::vector<math::Vector3f> &local_centers,
                                           const std::vector<math::Vector3f> &half_extents,
                                           const PhysicsBodyDesc &desc);

    // ── 范例 1：箱体堆叠与弹性反弹 ────────────────────────────────────────────

    struct BoxStackingScenario
    {
        Entity *ground = nullptr;            ///< 静态地面（top 面位于 z = 0）
        std::vector<Entity *> stack_boxes;   ///< 金字塔堆叠箱体（3-2-1 共 6 个，静摩擦堆叠）
        std::vector<Entity *> bouncy_boxes;  ///< 高恢复系数落箱（restitution ≈ 0.8，演示弹性反弹）
    };

    /**
     * 地面 + 3-2-1 金字塔堆叠 + 两枚高弹性落箱。
     * 金字塔箱体静置在 z=0.5/1.5/2.5 层；落箱从 z=8/12 高处释放。
     */
    BoxStackingScenario BuildBoxStackingScenario(ECSContext *ctx);

    // ── 范例 2：Z-up 胶囊体与复合形状斜坡滑滚 ────────────────────────────────

    struct CapsuleCompoundScenario
    {
        Entity *ground        = nullptr;   ///< 静态地面
        Entity *ramp          = nullptr;   ///< 静态斜坡（绕 X 轴 -20°，高端在 -Y 侧）
        Entity *capsule       = nullptr;   ///< 动态胶囊（局部 Z 轴对齐斜坡法线）
        Entity *compound_cart = nullptr;   ///< 动态复合体（箱 + 两侧轮箱的「小车」形状）
    };

    /**
     * 斜坡 + 坡顶胶囊 + 坡顶复合小车：模拟后两者应沿 +Y 方向下滑、Z 下降。
     */
    CapsuleCompoundScenario BuildCapsuleCompoundScenario(ECSContext *ctx);

    // ── 范例 3：运动学平台推挤 ───────────────────────────────────────────────

    struct KinematicPlatformScenario
    {
        Entity *ground           = nullptr;              ///< 静态地面
        Entity *platform         = nullptr;              ///< 运动学平台（Kinematic）
        Entity *push_wall        = nullptr;              ///< 静态挡墙（+Y 远端）
        std::vector<Entity *> riders;                    ///< 平台上的动态箱体
        math::Vector3f platform_start = math::Vector3f(0.0f, 0.0f, 0.75f);
        float        sweep_amplitude  = 4.0f;            ///< 平台沿 Y 正弦往复的幅度
    };

    KinematicPlatformScenario BuildKinematicPlatformScenario(ECSContext *ctx);

    /**
     * @brief 按时间驱动平台做正弦往复运动（ECS -> Kinematic 单向驱动的公共入口）。
     *
     * GUI 范例与无头测试都调用它，保证「逻辑位移 -> MoveKinematic」链路一致。
     */
    void DriveKinematicPlatform(ECSContext *ctx, const KinematicPlatformScenario &sc, float time_seconds);

    // ── 范例 4：射线击飞与传感器区域 ─────────────────────────────────────────

    struct RaycastSensorScenario
    {
        Entity *ground         = nullptr;              ///< 静态地面
        Entity *sensor         = nullptr;              ///< 传感器体积（is_sensor，SENSOR 层）
        math::Vector3f sensor_center = math::Vector3f(6.0f, 0.0f, 1.5f);
        math::Vector3f sensor_half   = math::Vector3f(1.5f, 1.5f, 1.5f);
        std::vector<Entity *> targets;                 ///< 地面散落的动态箱体（含一枚初始位于传感器区内）
        Entity *in_sensor_box  = nullptr;              ///< targets 中初始位于传感器体积内的那枚
    };

    RaycastSensorScenario BuildRaycastSensorScenario(ECSContext *ctx);

    /**
     * @brief 向物理世界发射射线并对命中实体施加瞬时冲量（击飞）。
     *
     * @return 命中实体指针；未命中返回 nullptr。
     */
    Entity *RaycastAndKnockback(JoltPhysicsSystem *physics,
                                const math::Vector3f &origin,
                                const math::Vector3f &direction,
                                float max_distance,
                                const math::Vector3f &impulse);

    /**
     * @brief 查询传感器体积内的实体集合（OverlapBox，用场景记录的几何参数）。
     *
     * 结果写入 out_entities（先 Clear）。返回命中数量。
     */
    size_t QuerySensorOverlap(JoltPhysicsSystem *physics,
                              const RaycastSensorScenario &sc,
                              std::vector<Entity *> &out_entities);

    // ── 步进工具（无头测试用；GUI 走 ECS Tick 自动驱动）──────────────────────

    /**
     * @brief 以固定步长步进物理系统 steps 次。
     *
     * 每个步长内依次执行 `extra_per_step`（可空）再 `physics->Update(step)`，
     * 用于 Kinematic 场景「先写 ECS 变换、后由物理系统同步」的时序复现。
     */
    void StepPhysics(JoltPhysicsSystem *physics, int steps, float step_seconds,
                     const std::function<void(int step_index, float time)> &extra_per_step = nullptr);
}//namespace hgl::ecs
