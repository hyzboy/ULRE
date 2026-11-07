#pragma once
// JoltPhysicsSystem.h —— JoltPhysics 物理后端及 ECS Tick 调度系统
//
// 核心职责：
//   1. 物理引擎生命周期管理：Jolt 基础库初始化、内存分配器、工作线程池与 PhysicsSystem；
//   2. 坐标与投影约定转换：引擎为 Vulkan 右手系 Z-up（重力默认 (0, 0, -9.81)）；
//      对齐 ModelCollisionShape::Capsule 沿局部 Z 轴与 Jolt 胶囊体沿 Y 轴的朝向映射；
//   3. 实体刚体自动创建：挂载 ModelCollisionProxyComponent 的实体自动装配 Jolt 碰撞体；
//      若同时挂载 RigidBodyComponent 则遵从其动静/物理属性，否则按 Transform 动静特性默认创建；
//   4. ECS 双向变换同步（ExecutionPhase::TickPhysics）：
//      - 模拟前（ECS -> Jolt）：Kinematic 刚体及手动瞬移实体的变换推送到物理引擎；
//      - 物理步进模拟（Step Simulation）；
//      - 模拟后（Jolt -> ECS）：Dynamic 刚体的模拟结果写回 TransformAccessor；
//   5. 物理场景查询：射线检测（Raycast）、重叠体查询（OverlapSphere、OverlapBox）。

#include <hgl/ecs/core/System.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/RigidBodyComponent.h>
#include <hgl/ecs/components/ModelCollisionProxyComponent.h>
#include <hgl/math/VectorTypes.h>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>

namespace JPH
{
    class BodyInterface;
    class PhysicsSystem;
}

namespace hgl::ecs
{
    /// 射线碰撞检测命中结果
    struct PhysicsRaycastHit
    {
        bool           has_hit   = false;
        EntityID       entity_id = 0;
        Entity        *entity    = nullptr;
        math::Vector3f position  = math::Vector3f(0.0f);
        math::Vector3f normal    = math::Vector3f(0.0f);
        float          distance  = 0.0f;
        float          fraction  = 0.0f;
        uint32_t       body_id   = 0xFFFFFFFFu;
    };

    /// 空间重叠查询命中结果
    struct PhysicsOverlapHit
    {
        EntityID entity_id = 0;
        Entity  *entity    = nullptr;
        uint32_t body_id   = 0xFFFFFFFFu;
    };

    /**
     * @brief Jolt 物理系统
     */
    class JoltPhysicsSystem : public System
    {
    private:

        ECSContext *world = nullptr;

        // PIMPL 模式管理 Jolt 引擎实例与内部数据结构
        class Impl;
        std::unique_ptr<Impl> impl;

    public:

        JoltPhysicsSystem(const std::string &name = "JoltPhysicsSystem");
        ~JoltPhysicsSystem() override;

    public:

        void SetWorld(ECSContext *w) { world = w; SetContext(w); }
        ECSContext *GetWorld() const { return world ? world : context; }

        void Initialize() override;
        void Shutdown() override;
        void Update(float deltaTime) override;

    public: // 刚体与碰撞体管理

        /// 为指定实体创建或更新 Jolt 物理刚体（返回创建的 Jolt BodyID 整数编码）
        uint32_t CreateOrUpdateBody(Entity *entity);

        /// 销毁指定实体的 Jolt 刚体
        void DestroyBody(Entity *entity);

        /// 销毁指定刚体组件关联的 Jolt 刚体
        void DestroyBody(RigidBodyComponent *rb);

        /// 重新激活指定实体的刚体
        void ActivateBody(uint32_t body_id);
        void DeactivateBody(uint32_t body_id);
        bool IsBodyActive(uint32_t body_id) const;

        /// 读写速度与施加力/冲量
        void SetLinearVelocity(uint32_t body_id, const math::Vector3f &velocity);
        math::Vector3f GetLinearVelocity(uint32_t body_id) const;

        void SetAngularVelocity(uint32_t body_id, const math::Vector3f &angular_velocity);
        math::Vector3f GetAngularVelocity(uint32_t body_id) const;

        void AddForce(uint32_t body_id, const math::Vector3f &force);
        void AddImpulse(uint32_t body_id, const math::Vector3f &impulse);
        void AddTorque(uint32_t body_id, const math::Vector3f &torque);
        void AddAngularImpulse(uint32_t body_id, const math::Vector3f &angular_impulse);

        void SetPositionAndRotation(uint32_t body_id, const math::Vector3f &pos, const glm::quat &rot, bool activate = true);

    public: // 物理场景查询 API

        /// 射线检测（点 + 归一化方向 + 最大距离）
        bool Raycast(const math::Vector3f &origin, const math::Vector3f &direction, float max_distance, PhysicsRaycastHit &out_hit) const;

        /// 线段检测（起点 + 终点）
        bool RaycastSegment(const math::Vector3f &start, const math::Vector3f &end, PhysicsRaycastHit &out_hit) const;

        /// 球体空间重叠检测
        void OverlapSphere(const math::Vector3f &center, float radius, std::vector<PhysicsOverlapHit> &out_hits) const;

        /// 有向包围盒空间重叠检测
        void OverlapBox(const math::Vector3f &center, const math::Vector3f &half_extents, const glm::quat &rotation, std::vector<PhysicsOverlapHit> &out_hits) const;

    public: // 底层接口暴露

        JPH::PhysicsSystem *GetJoltPhysicsSystem();
        JPH::BodyInterface *GetJoltBodyInterface();

    public: // 参数设置

        void SetGravity(const math::Vector3f &gravity);
        math::Vector3f GetGravity() const;
    };
}//namespace hgl::ecs
