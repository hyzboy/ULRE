#include<hgl/ecs/CameraSystem.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/InputSystem.h>
#include<hgl/graph/camera/Camera.h>
#include<hgl/graph/ViewportInfo.h>
#include<hgl/math/Math.h>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>

namespace hgl::ecs
{
    CameraSystem::CameraSystem(ECSContext* ctx)
        : context(ctx)
        , input_system(nullptr)
    {
    }

    void CameraSystem::Update(float deltaTime)
    {
        if (!context)
            return;

        // 获取InputSystem（首次调用时查找）
        if (!input_system)
        {
            input_system = context->GetSystem<InputSystem>().get();
        }

        // 收集所有摄像机
        auto cameras = CollectCameras();
        if (cameras.empty())
            return;

        // 收集输入状态
        CollectInput();

        // 处理每个摄像机
        for (auto& camera_comp : cameras)
        {
            if (!camera_comp)
                continue;

            // 处理输入
            ProcessInput(camera_comp.get(), deltaTime);

            // 更新局部坐标系
            UpdateBasis(camera_comp.get());

            // 更新位置和目标
            UpdateTransform(camera_comp.get());

            // 更新矩阵
            UpdateMatrices(camera_comp.get());

            // 上传到GPU
            UploadToGPU(camera_comp.get());
        }
    }

    std::vector<std::shared_ptr<CameraComponent>> CameraSystem::CollectCameras()
    {
        std::vector<std::shared_ptr<CameraComponent>> result;
        if (context)
        {
            context->GetComponents<CameraComponent>(result);
        }
        return result;
    }

    void CameraSystem::CollectInput()
    {
        if (!input_system)
            return;

        // 更新鼠标位置
        input_state.last_mouse_pos = input_state.mouse_pos;
        input_state.mouse_pos = input_system->GetMouseCoord();
        input_state.mouse_delta = input_state.mouse_pos - input_state.last_mouse_pos;

        // 获取鼠标按钮状态
        input_state.left_button = input_system->IsMouseButtonDown(0);
        input_state.right_button = input_system->IsMouseButtonDown(1);
        input_state.middle_button = input_system->IsMouseButtonDown(2);

        // 获取滚轮增量
        input_state.wheel_delta = input_system->GetWheelDelta();

        // 获取键盘状态 (使用ASCII码)
        input_state.key_w = input_system->IsKeyDown('W') || input_system->IsKeyDown('w');
        input_state.key_s = input_system->IsKeyDown('S') || input_system->IsKeyDown('s');
        input_state.key_a = input_system->IsKeyDown('A') || input_system->IsKeyDown('a');
        input_state.key_d = input_system->IsKeyDown('D') || input_system->IsKeyDown('d');
        input_state.key_q = input_system->IsKeyDown('Q') || input_system->IsKeyDown('q');
        input_state.key_e = input_system->IsKeyDown('E') || input_system->IsKeyDown('e');
    }

    void CameraSystem::ProcessInput(CameraComponent* camera, float deltaTime)
    {
        if (!camera)
            return;

        switch (camera->control_mode)
        {
            case CameraComponent::ControlMode::FirstPerson:
                ProcessFirstPersonInput(camera, deltaTime);
                break;

            case CameraComponent::ControlMode::ViewModel:
                ProcessViewModelInput(camera, deltaTime);
                break;

            case CameraComponent::ControlMode::LookAt:
                ProcessLookAtInput(camera, deltaTime);
                break;

            case CameraComponent::ControlMode::Free:
                // 自由模式不处理输入
                break;
        }
    }

    void CameraSystem::ProcessFirstPersonInput(CameraComponent* camera, float deltaTime)
    {
        if (!camera)
            return;

        bool input_changed = false;

        // 鼠标旋转
        if (input_state.left_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
        {
            camera->yaw += input_state.mouse_delta.x * camera->rotation_sensitivity * camera->input_invert.x;
            camera->pitch -= input_state.mouse_delta.y * camera->rotation_sensitivity * camera->input_invert.y;

            // 限制俯仰角
            if (camera->pitch > 89.0f) camera->pitch = 89.0f;
            if (camera->pitch < -89.0f) camera->pitch = -89.0f;

            input_changed = true;
        }

        // WASD移动
        math::Vector3f movement(0.0f, 0.0f, 0.0f);

        if (input_state.key_w)
            movement += camera->forward;
        if (input_state.key_s)
            movement -= camera->forward;
        if (input_state.key_d)
            movement += camera->right;
        if (input_state.key_a)
            movement -= camera->right;
        if (input_state.key_e)
            movement += camera->up;
        if (input_state.key_q)
            movement -= camera->up;

        if (length(movement) > 0.001f)
        {
            movement = normalize(movement) * camera->move_speed * deltaTime;
            camera->position += movement;
            input_changed = true;
        }

        if (input_changed)
        {
            camera->matrix_dirty = true;
        }
    }

    void CameraSystem::ProcessViewModelInput(CameraComponent* camera, float deltaTime)
    {
        if (!camera)
            return;

        bool input_changed = false;

        // 左键拖拽旋转
        if (input_state.left_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
        {
            camera->yaw += input_state.mouse_delta.x * camera->rotation_sensitivity * camera->input_invert.x;
            camera->pitch -= input_state.mouse_delta.y * camera->rotation_sensitivity * camera->input_invert.y;

            // 限制俯仰角
            if (camera->pitch > 89.0f) camera->pitch = 89.0f;
            if (camera->pitch < -89.0f) camera->pitch = -89.0f;

            input_changed = true;
        }

        // 滚轮缩放距离
        if (input_state.wheel_delta != 0)
        {
            camera->distance -= input_state.wheel_delta * camera->zoom_sensitivity;
            
            // 限制距离
            if (camera->distance < camera->min_distance)
                camera->distance = camera->min_distance;
            if (camera->distance > camera->max_distance)
                camera->distance = camera->max_distance;

            input_changed = true;
        }

        // 右键平移
        if (input_state.right_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
        {
            float pan_speed = 0.01f * camera->distance;
            math::Vector3f pan_offset = 
                camera->right * (-input_state.mouse_delta.x * pan_speed) +
                camera->up * (input_state.mouse_delta.y * pan_speed);

            camera->target += pan_offset;
            input_changed = true;
        }

        if (input_changed)
        {
            camera->matrix_dirty = true;
        }
    }

    void CameraSystem::ProcessLookAtInput(CameraComponent* camera, float deltaTime)
    {
        if (!camera)
            return;

        bool input_changed = false;

        // 中键平移
        if (input_state.middle_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
        {
            float pan_speed = 0.01f * camera->distance;
            math::Vector3f pan_offset = 
                camera->right * (-input_state.mouse_delta.x * pan_speed) +
                camera->up * (input_state.mouse_delta.y * pan_speed);

            camera->target += pan_offset;
            input_changed = true;
        }

        // 滚轮调整距离
        if (input_state.wheel_delta != 0)
        {
            camera->distance -= input_state.wheel_delta * camera->zoom_sensitivity;
            
            // 限制距离
            if (camera->distance < camera->min_distance)
                camera->distance = camera->min_distance;
            if (camera->distance > camera->max_distance)
                camera->distance = camera->max_distance;

            input_changed = true;
        }

        if (input_changed)
        {
            camera->matrix_dirty = true;
        }
    }

    void CameraSystem::UpdateBasis(CameraComponent* camera)
    {
        if (!camera)
            return;

        // 从欧拉角计算前向向量
        camera->forward = ComputeForward(camera->yaw, camera->pitch);

        // 计算右向和上向向量
        ComputeRightUp(camera->forward, camera->world_up, camera->right, camera->up);
    }

    void CameraSystem::UpdateTransform(CameraComponent* camera)
    {
        if (!camera)
            return;

        // 根据控制模式更新位置或目标
        switch (camera->control_mode)
        {
            case CameraComponent::ControlMode::FirstPerson:
                // 第一人称：target = position + forward
                camera->target = camera->position + camera->forward;
                break;

            case CameraComponent::ControlMode::ViewModel:
            case CameraComponent::ControlMode::LookAt:
                // ViewModel/LookAt：position = target - forward * distance
                camera->position = camera->target - camera->forward * camera->distance;
                break;

            case CameraComponent::ControlMode::Free:
                // 自由模式：不自动更新
                break;
        }
    }

    void CameraSystem::UpdateMatrices(CameraComponent* camera)
    {
        if (!camera || !camera->matrix_dirty)
            return;

        // 更新camera_data
        if (camera->camera_data)
        {
            camera->camera_data->pos = camera->position;
            camera->camera_data->viewDirection = camera->forward;
            camera->camera_data->world_up = camera->world_up;
            camera->camera_data->fovY = camera->fov;
            camera->camera_data->znear = camera->near_plane;
            camera->camera_data->zfar = camera->far_plane;
        }

        // 更新camera_info
        if (camera->camera_info && camera->viewport_info && camera->camera_data)
        {
            // 计算视图矩阵
            camera->camera_info->view = math::LookAtMatrix(
                camera->position,
                camera->target,
                camera->world_up
            );

            // 调用RefreshCameraInfo更新所有矩阵
            graph::RefreshCameraInfo(
                camera->camera_info,
                camera->viewport_info,
                camera->camera_data
            );
        }

        camera->matrix_dirty = false;
    }

    void CameraSystem::UploadToGPU(CameraComponent* camera)
    {
        if (!camera || !camera->camera_ubo || !camera->camera_info)
            return;

        // 上传CameraInfo到GPU
        // 注意：这里假设camera_ubo有Write方法，实际实现可能需要调整
        // camera->camera_ubo->Write(camera->camera_info, sizeof(graph::CameraInfo));
    }

    // === 数学辅助函数 ===

    math::Vector3f CameraSystem::ComputeForward(float yaw, float pitch)
    {
        // 从欧拉角计算前向向量
        // yaw: 绕Z轴旋转, pitch: 绕Y轴旋转
        float yaw_rad = glm::radians(yaw);
        float pitch_rad = glm::radians(pitch);

        math::Vector3f forward;
        forward.x = cos(pitch_rad) * cos(yaw_rad);
        forward.y = cos(pitch_rad) * sin(yaw_rad);
        forward.z = sin(pitch_rad);

        return normalize(forward);
    }

    void CameraSystem::ComputeRightUp(const math::Vector3f& forward,
                                      const math::Vector3f& world_up,
                                      math::Vector3f& right,
                                      math::Vector3f& up)
    {
        // 计算右向量: right = normalize(forward × world_up)
        right = normalize(cross(forward, world_up));

        // 计算上向量: up = normalize(right × forward)
        up = normalize(cross(right, forward));
    }
}//namespace hgl::ecs
