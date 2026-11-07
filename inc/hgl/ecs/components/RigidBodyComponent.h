#pragma once
// RigidBodyComponent.h —— 物理刚体组件声明
//
// 角色与职责：
//   · 配合 ModelCollisionProxyComponent 提供物理刚体模拟属性；
//   · 负责 Static / Kinematic / Dynamic 物理体分类及物理参数（质量、阻尼、摩擦力、弹性）；
//   · 内部记录 Jolt BodyID 与 JoltPhysicsSystem 引用；
//   · 提供施加力、冲量、线速度/角速度读写等接口；
//   · 支持 OnDetach 回收（在组件或实体销毁时自动通知物理引擎销毁 Jolt 刚体）。

#include <hgl/ecs/core/entity/ComponentRow.h>
#include <hgl/ecs/support/storage/ComponentTypeTable.h>
#include <hgl/math/VectorTypes.h>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>

namespace hgl::ecs
{
    class Entity;
    class JoltPhysicsSystem;

    /// 刚体运动学类型
    enum class BodyMotionType : uint8_t
    {
        Static = 0,    ///< 静态刚体（场景固定物、地面、建筑，不受外力也不更新 Transform）
        Kinematic = 1, ///< 动力学运动学刚体（受 ECS 逻辑变换驱动，推挤动态刚体，速度由逻辑位移决定）
        Dynamic = 2    ///< 动态刚体（受重力、碰撞冲量、外力驱动，模拟结果写回 ECS Transform）
    };

    /**
     * @brief 物理刚体组件（CPU-only，无 GPU 行）
     */
    struct RigidBodyComponent : public ComponentRow
    {
        BodyMotionType motion_type = BodyMotionType::Static;

        float mass              = 1.0f;     ///< 质量（千克，仅 Dynamic 有效）
        float friction          = 0.2f;     ///< 摩擦因数 [0, 1]
        float restitution       = 0.0f;     ///< 弹性恢复因数 [0, 1]
        float linear_damping    = 0.05f;    ///< 线性阻尼
        float angular_damping   = 0.05f;    ///< 角阻尼
        float gravity_factor    = 1.0f;     ///< 重力缩放因数
        bool  is_sensor         = false;    ///< 是否为传感器/触发器（只产生重叠事件，不产生物理碰撞阻挡）
        bool  allow_sleeping    = true;     ///< 是否允许静止休眠

        /// Jolt 刚体底层 ID（内部管理，JPH::BodyID::GetIndexAndSequenceNumber()）
        uint32_t jolt_body_id = 0xFFFFFFFFu;

        /// 所属物理系统（内部管理，用于 OnDetach 自动销毁刚体）
        JoltPhysicsSystem *physics_system = nullptr;

        RigidBodyComponent() = default;
        ~RigidBodyComponent() = default;

        /// 卸载钩子（由 ComponentPool 在 DestroyRowAt 时自动调用）
        void OnDetach();

        /// 显式销毁持有的物理刚体
        void DestroyBody();

        /// 是否持有有效的 Jolt 刚体
        bool IsValidBody() const { return jolt_body_id != 0xFFFFFFFFu; }

        /// 刚体激活状态查询与设置
        bool IsActive() const;
        void Activate();
        void Deactivate();

        /// 速度读写
        void SetLinearVelocity(const math::Vector3f &velocity);
        math::Vector3f GetLinearVelocity() const;

        void SetAngularVelocity(const math::Vector3f &angular_velocity);
        math::Vector3f GetAngularVelocity() const;

        /// 动力学受力与冲量
        void AddForce(const math::Vector3f &force);
        void AddImpulse(const math::Vector3f &impulse);
        void AddTorque(const math::Vector3f &torque);
        void AddAngularImpulse(const math::Vector3f &angular_impulse);

        /// 立即传送/设置物理体位置和姿态
        void SetPositionAndRotation(const math::Vector3f &position, const glm::quat &rotation, bool activate = true);
    };

    /// 获取实体的物理刚体组件（未挂载 ⇒ nullptr）
    RigidBodyComponent *GetRigidBody(Entity *owner);
    const RigidBodyComponent *GetRigidBody(const Entity *owner);

    /// 登记组件类型号
    template<>
    struct ComponentTypeOf<RigidBodyComponent>
    {
        static constexpr ComponentType value = ComponentType::RigidBody;
    };
}//namespace hgl::ecs
