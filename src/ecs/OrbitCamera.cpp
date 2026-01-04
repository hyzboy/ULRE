#include<hgl/ecs/OrbitCamera.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/math/MatrixOperations.h>
#include<glm/gtc/matrix_transform.hpp>
#include<iostream>

namespace hgl::ecs
{
    // ============================================================================
    // OrbitCamera Implementation
    // ============================================================================

    void OrbitCamera::UpdateCameraMatrix()
    {
        if (!camera || !camera_info)
            return;

        camera->viewDirection = Normalized(camera->pos - target);
        camera_info->view = glm::lookAtRH(
            glm::vec3(camera->pos.x, camera->pos.y, camera->pos.z),
            glm::vec3(target.x, target.y, target.z),
            glm::vec3(camera->world_up.x, camera->world_up.y, camera->world_up.z)
        );

        direction = camera->viewDirection;
        right = Normalized(Cross(camera->world_up, direction));
        up = Normalized(Cross(right, direction));
    }

    void OrbitCamera::Rotate(double ang, const Vector3f& axis)
    {
        if (!camera)
            return;

        Vector3f a = axis;
        Normalize(a);

        Matrix3f rot = hgl::math::AxisRotate3Deg(static_cast<float>(ang), Vector3f(a.x, a.y, a.z));

        target = camera->pos + rot * (target - camera->pos);
    }

    void OrbitCamera::OrbitRotate(double ang, const Vector3f& axis)
    {
        if (!camera)
            return;

        Vector3f a = axis;
        Normalize(a);

        Matrix3f rot = hgl::math::AxisRotate3Deg(static_cast<float>(ang), Vector3f(a.x, a.y, a.z));

        camera->pos = target + rot * (camera->pos - target);
    }
} // namespace hgl::ecs
