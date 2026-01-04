#include<hgl/ecs/ModelViewerCamera.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/math/MatrixOperations.h>
#include<glm/gtc/matrix_transform.hpp>
#include<iostream>

namespace hgl::ecs
{
    // ============================================================================
    // ModelViewerCamera Implementation
    // ============================================================================

    void ModelViewerCamera::UpdateCameraMatrix()
    {
        if (!camera || !camera_info)
            return;

        if (dirty_basis)
        {
            RecalcBasis(yaw, pitch, forward, right, up, camera);
            camera->pos = target - forward * distance;
            dirty_basis = false;
        }

        camera_info->view = glm::lookAtRH(
            glm::vec3(camera->pos.x, camera->pos.y, camera->pos.z),
            glm::vec3(target.x, target.y, target.z),
            glm::vec3(camera->world_up.x, camera->world_up.y, camera->world_up.z)
        );
    }

    void ModelViewerCamera::Rotate(const Vector2f& delta_pixels)
    {
        const float sens_rad = deg2rad(rotation_sensitivity_deg_per_pixel);
        yaw -= delta_pixels.x * input_invert_sign.x * sens_rad;
        pitch -= delta_pixels.y * input_invert_sign.y * sens_rad;

        // Clamp pitch to avoid singularities
        const float top = deg2rad(89.0f);
        const float bottom = deg2rad(-89.0f);
        if (pitch > top) pitch = top;
        if (pitch < bottom) pitch = bottom;

        // Normalize yaw
        const float PI = 3.14159265358979323846f;
        yaw = fmodf(yaw, 2.0f * PI);
        if (yaw > PI) yaw -= 2.0f * PI;
        else if (yaw < -PI) yaw += 2.0f * PI;

        UpdateCameraVector();
    }

    void ModelViewerCamera::Zoom(float wheel_delta)
    {
        // wheel_delta is integer steps (positive -> away)
        distance *= std::pow(1.0f + zoom_sensitivity, -wheel_delta);
        distance = std::clamp(distance, min_distance, max_distance);
        UpdateCameraVector();
    }

    // ============================================================================
    // ModelViewerMouseInput Implementation
    // ============================================================================

    io::EventProcResult ModelViewerMouseInput::OnPressed(const Vector2i& mouse_coord, io::MouseButton mb)
    {
        mouse_last = Vector2f(mouse_coord);
        dragging = true;
        return io::EventProcResult::Continue;
    }

    io::EventProcResult ModelViewerMouseInput::OnMove(const Vector2i& mouse_coord)
    {
        if (!dragging || !camera)
            return io::EventProcResult::Continue;

        Vector2f pos(mouse_coord);
        Vector2f delta = pos - mouse_last;
        mouse_last = pos;

        if (HasPressed(io::MouseButton::Left))
        {
            camera->Rotate(delta);
            camera->UpdateCameraVector();
        }

        return io::EventProcResult::Continue;
    }

    io::EventProcResult ModelViewerMouseInput::OnWheel(const Vector2i& mouse_coord)
    {
        if (camera && mouse_coord.y != 0)
        {
            camera->Zoom(float(mouse_coord.y));
            camera->UpdateCameraVector();
        }
        return io::EventProcResult::Continue;
    }

    io::EventProcResult ModelViewerMouseInput::OnReleased(const Vector2i& mouse_coord, io::MouseButton)
    {
        dragging = false;
        return io::EventProcResult::Continue;
    }
} // namespace hgl::ecs
