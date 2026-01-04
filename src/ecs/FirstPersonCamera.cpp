#include<hgl/ecs/FirstPersonCamera.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/math/MatrixOperations.h>
#include<glm/gtc/matrix_transform.hpp>
#include<iostream>

namespace hgl::ecs
{
    // ============================================================================
    // FirstPersonCamera Implementation
    // ============================================================================

    void FirstPersonCamera::UpdateCameraVector()
    {
        if (!camera)
            return;

        const float cp = cosf(pitch);
        forward.x = cp * cosf(yaw);
        forward.y = cp * sinf(yaw);
        forward.z = sinf(pitch);

        Vector3f world_up = camera->world_up;

        right = Normalized(Cross(forward, world_up));
        up = Normalized(Cross(right, forward));
    }

    void FirstPersonCamera::Rotate(const Vector2f& axis)
    {
        const float sens_rad = deg2rad(rotation_sensitivity_deg_per_pixel);
        yaw -= axis.x * input_invert_sign.x * sens_rad;
        pitch -= axis.y * input_invert_sign.y * sens_rad;

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

    void FirstPersonCamera::UpdateCameraMatrix()
    {
        if (!camera || !camera_info)
            return;

        UpdateCameraVector();

        // Build view matrix from forward, right, up vectors
        camera_info->view = glm::lookAtRH(
            glm::vec3(camera->pos.x, camera->pos.y, camera->pos.z),
            glm::vec3(camera->pos.x + forward.x, camera->pos.y + forward.y, camera->pos.z + forward.z),
            glm::vec3(camera->world_up.x, camera->world_up.y, camera->world_up.z)
        );
    }

    io::EventProcResult FirstPersonKeyboardInput::OnPressed(const io::KeyboardButton& kb)
    {
        return io::EventProcResult::Continue;
    }

    bool FirstPersonKeyboardInput::Update()
    {
        // Keyboard handling would go here if needed
        return true;
    }

    io::EventProcResult FirstPersonMouseInput::OnPressed(const Vector2i& mouse_coord, io::MouseButton)
    {
        mouse_pos = Vector2f(mouse_coord);
        mouse_last_pos = mouse_pos;
        return io::EventProcResult::Continue;
    }

    io::EventProcResult FirstPersonMouseInput::OnWheel(const Vector2i& mouse_coord)
    {
        if (camera && mouse_coord.y != 0)
        {
            camera->ZoomFOV(mouse_coord.y);
        }
        return io::EventProcResult::Continue;
    }

    io::EventProcResult FirstPersonMouseInput::OnMove(const Vector2i& mouse_coord)
    {
        Vector2f pos(mouse_coord);
        Vector2f delta = pos - mouse_last_pos;
        mouse_last_pos = pos;
        mouse_pos = pos;

        if (camera && HasPressed(io::MouseButton::Right))
        {
            camera->Rotate(delta);
        }

        return io::EventProcResult::Continue;
    }
} // namespace hgl::ecs
