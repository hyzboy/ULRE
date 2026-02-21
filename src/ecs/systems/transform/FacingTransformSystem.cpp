#include<hgl/ecs/systems/transform/FacingTransformSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/FacingTransformComponent.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/graph/CameraInfo.h>
#include<glm/gtx/quaternion.hpp>
#include<iostream>
#include<cmath>

namespace hgl::ecs
{
    FacingTransformSystem::FacingTransformSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::Transform);
        SetExecutionOrder(ExecutionPhase::TickPostCamera);
        AddDependency<TransformSystem>();
        AddDependency<CameraSystem>();
    }

    void FacingTransformSystem::Update(float deltaTime)
    {
        if (!world || !camera_info)
            return;

        // Get all entities and iterate through them
        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto facing = entity->GetComponent<FacingTransformComponent>();
            if (!facing || !facing->IsEnabled())
                continue;

            auto transform = entity->GetComponent<TransformComponent>();
            if (!transform)
                continue;

            // Calculate and apply facing rotation
            UpdateFacingRotation(facing.get(), transform.get(), deltaTime);
        }
    }

    bool FacingTransformSystem::UpdateFacingRotation(FacingTransformComponent* facing,
                                                     TransformComponent* transform,
                                                     float deltaTime)
    {
        if (!facing || !transform || !camera_info)
            return false;

        FacingMode mode = facing->GetFacingMode();

        switch (mode)
        {
            case FacingMode::LookAtCamera:
                return CalculateLookAtCameraRotation(transform, deltaTime);

            case FacingMode::LookAtTarget:
                return CalculateLookAtTargetRotation(transform, facing->GetTargetPosition(), deltaTime);

            case FacingMode::BillboardY:
                return CalculateBillboardYRotation(transform, deltaTime);

            case FacingMode::BillboardZ:
                return CalculateBillboardZRotation(transform, deltaTime);

            default:
                return false;
        }
    }

    bool FacingTransformSystem::CalculateLookAtCameraRotation(TransformComponent* transform,
                                                              float deltaTime)
    {
        if (!transform || !camera_info)
            return false;

        try
        {
            // Get world position
            glm::vec3 world_pos = glm::vec3(transform->GetWorldMatrix()[3]);
            glm::vec3 camera_pos = glm::vec3(camera_info->pos.x, camera_info->pos.y, camera_info->pos.z);

            // Calculate direction from entity to camera
            const glm::vec3 camera_delta = camera_pos - world_pos;
            const float dist2 = glm::dot(camera_delta, camera_delta);
            if (dist2 < 1e-6f)
                return false;

            glm::vec3 direction = camera_delta / std::sqrt(dist2);

            // Z-up stable basis construction
            glm::vec3 up(0.0f, 0.0f, 1.0f);
            if (std::abs(glm::dot(direction, up)) > 0.99f)
                up = glm::vec3(0.0f, 1.0f, 0.0f);

            glm::vec3 forward = -direction;
            glm::vec3 right = glm::normalize(glm::cross(up, forward));
            glm::vec3 calc_up = glm::cross(forward, right);

            // Create rotation matrix
            glm::mat4 rotation_matrix(1.0f);
            rotation_matrix[0] = glm::vec4(right, 0.0f);
            rotation_matrix[1] = glm::vec4(calc_up, 0.0f);
            rotation_matrix[2] = glm::vec4(forward, 0.0f);

            // Extract quaternion from rotation matrix
            glm::quat new_rotation = glm::quat_cast(rotation_matrix);
            if (!std::isfinite(new_rotation.w) || !std::isfinite(new_rotation.x) ||
                !std::isfinite(new_rotation.y) || !std::isfinite(new_rotation.z))
                return false;

            // Apply rotation to transform
            transform->SetLocalRotation(new_rotation);

            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool FacingTransformSystem::CalculateLookAtTargetRotation(TransformComponent* transform,
                                                              const glm::vec3& target_pos,
                                                              float deltaTime)
    {
        if (!transform)
            return false;

        try
        {
            glm::vec3 world_pos = glm::vec3(transform->GetWorldMatrix()[3]);
            glm::vec3 to_target = target_pos - world_pos;
            float dist2 = glm::dot(to_target, to_target);

            if (dist2 < 1e-6f)
                return false;

            glm::vec3 direction = glm::normalize(to_target);
            glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);

            // Avoid gimbal lock if looking straight up/down
            if (std::abs(glm::dot(direction, up)) > 0.99f)
            {
                up = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            // Create look-at rotation matrix
            glm::vec3 forward = -direction;
            glm::vec3 right = glm::normalize(glm::cross(up, forward));
            glm::vec3 calc_up = glm::cross(forward, right);

            glm::mat4 rotation_matrix(1.0f);
            rotation_matrix[0] = glm::vec4(right, 0.0f);
            rotation_matrix[1] = glm::vec4(calc_up, 0.0f);
            rotation_matrix[2] = glm::vec4(forward, 0.0f);

            glm::quat new_rotation = glm::quat_cast(rotation_matrix);
            if (!std::isfinite(new_rotation.w) || !std::isfinite(new_rotation.x) ||
                !std::isfinite(new_rotation.y) || !std::isfinite(new_rotation.z))
                return false;

            transform->SetLocalRotation(new_rotation);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool FacingTransformSystem::CalculateBillboardYRotation(TransformComponent* transform,
                                                           float deltaTime)
    {
        if (!transform || !camera_info)
            return false;

        try
        {
            glm::vec3 world_pos = glm::vec3(transform->GetWorldMatrix()[3]);
            glm::vec3 camera_pos = glm::vec3(camera_info->pos.x, camera_info->pos.y, camera_info->pos.z);

            // Only rotate around Y axis (vertical)
            glm::vec3 forward = glm::normalize(glm::vec3(camera_pos.x - world_pos.x,
                                                         0.0f,  // Ignore Y difference
                                                         camera_pos.z - world_pos.z));

            if (glm::dot(forward, forward) < 1e-6f)
                return false;

            glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), forward));
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

            glm::mat4 rotation_matrix(1.0f);
            rotation_matrix[0] = glm::vec4(right, 0.0f);
            rotation_matrix[1] = glm::vec4(up, 0.0f);
            rotation_matrix[2] = glm::vec4(-forward, 0.0f);

            glm::quat new_rotation = glm::quat_cast(rotation_matrix);
            if (!std::isfinite(new_rotation.w) || !std::isfinite(new_rotation.x) ||
                !std::isfinite(new_rotation.y) || !std::isfinite(new_rotation.z))
                return false;

            transform->SetLocalRotation(new_rotation);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }

    bool FacingTransformSystem::CalculateBillboardZRotation(TransformComponent* transform,
                                                           float deltaTime)
    {
        if (!transform || !camera_info)
            return false;

        try
        {
            glm::vec3 world_pos = glm::vec3(transform->GetWorldMatrix()[3]);
            glm::vec3 camera_pos = glm::vec3(camera_info->pos.x, camera_info->pos.y, camera_info->pos.z);

            // Z-up billboard: only rotate around Z axis (pure yaw, no pitch/roll change)
            glm::vec3 to_camera = glm::vec3(camera_pos.x - world_pos.x,
                                            camera_pos.y - world_pos.y,
                                            0.0f); // Ignore Z difference

            const float len2 = glm::dot(to_camera, to_camera);
            if (len2 < 1e-6f)
                return false;

            // Calculate yaw angle in XY plane to point toward camera
            float yaw = std::atan2(to_camera.y, to_camera.x);

            // Create rotation quaternion around Z axis only
            // Quaternion for rotation θ around Z(0,0,1): q = (cos(θ/2), 0, 0, sin(θ/2))
            float half_yaw = yaw * 0.5f;
            glm::quat new_rotation(std::cos(half_yaw), 0.0f, 0.0f, std::sin(half_yaw));
            if (!std::isfinite(new_rotation.w) || !std::isfinite(new_rotation.x) ||
                !std::isfinite(new_rotation.y) || !std::isfinite(new_rotation.z))
                return false;

            transform->SetLocalRotation(new_rotation);
            return true;
        }
        catch (const std::exception&)
        {
            return false;
        }
    }
}//namespace hgl::ecs
