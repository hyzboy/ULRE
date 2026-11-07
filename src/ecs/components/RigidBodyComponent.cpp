#include <hgl/ecs/components/RigidBodyComponent.h>
#include <hgl/ecs/systems/tick/JoltPhysicsSystem.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::ecs
{
    void RigidBodyComponent::OnDetach()
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
        {
            physics_system->DestroyBody(this);
        }
        physics_system = nullptr;
        jolt_body_id = 0xFFFFFFFFu;
    }

    void RigidBodyComponent::DestroyBody()
    {
        OnDetach();
    }

    bool RigidBodyComponent::IsActive() const
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            return physics_system->IsBodyActive(jolt_body_id);
        return false;
    }

    void RigidBodyComponent::Activate()
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->ActivateBody(jolt_body_id);
    }

    void RigidBodyComponent::Deactivate()
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->DeactivateBody(jolt_body_id);
    }

    void RigidBodyComponent::SetLinearVelocity(const math::Vector3f &velocity)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->SetLinearVelocity(jolt_body_id, velocity);
    }

    math::Vector3f RigidBodyComponent::GetLinearVelocity() const
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            return physics_system->GetLinearVelocity(jolt_body_id);
        return math::Vector3f(0.0f);
    }

    void RigidBodyComponent::SetAngularVelocity(const math::Vector3f &angular_velocity)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->SetAngularVelocity(jolt_body_id, angular_velocity);
    }

    math::Vector3f RigidBodyComponent::GetAngularVelocity() const
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            return physics_system->GetAngularVelocity(jolt_body_id);
        return math::Vector3f(0.0f);
    }

    void RigidBodyComponent::AddForce(const math::Vector3f &force)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->AddForce(jolt_body_id, force);
    }

    void RigidBodyComponent::AddImpulse(const math::Vector3f &impulse)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->AddImpulse(jolt_body_id, impulse);
    }

    void RigidBodyComponent::AddTorque(const math::Vector3f &torque)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->AddTorque(jolt_body_id, torque);
    }

    void RigidBodyComponent::AddAngularImpulse(const math::Vector3f &angular_impulse)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->AddAngularImpulse(jolt_body_id, angular_impulse);
    }

    void RigidBodyComponent::SetPositionAndRotation(const math::Vector3f &position, const glm::quat &rotation, bool activate)
    {
        if (physics_system && jolt_body_id != 0xFFFFFFFFu)
            physics_system->SetPositionAndRotation(jolt_body_id, position, rotation, activate);
    }

    RigidBodyComponent *GetRigidBody(Entity *owner)
    {
        if (!owner)
            return nullptr;
        return owner->GetComponent<RigidBodyComponent>();
    }

    const RigidBodyComponent *GetRigidBody(const Entity *owner)
    {
        if (!owner)
            return nullptr;
        return owner->GetComponent<RigidBodyComponent>();
    }
}//namespace hgl::ecs
