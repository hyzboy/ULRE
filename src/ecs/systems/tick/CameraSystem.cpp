#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/mtl/UBOCommon.h>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>
#include<cmath>
#include<iostream>

namespace hgl::ecs
{
    namespace
    {
        class FirstPersonCameraMode final : public CameraModeProcessor
        {
        public:
            CameraComponent::ControlMode GetMode() const override
            {
                return CameraComponent::ControlMode::FirstPerson;
            }

            void ProcessInput(CameraComponent* camera, const CameraInputState& input_state, float deltaTime) override
            {
                if (!camera)
                    return;

                bool input_changed = false;

                if (input_state.left_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
                {
                    camera->yaw += input_state.mouse_delta.x * camera->rotation_sensitivity * camera->input_invert.x;
                    camera->pitch -= input_state.mouse_delta.y * camera->rotation_sensitivity * camera->input_invert.y;

                    if (camera->pitch > 89.0f) camera->pitch = 89.0f;
                    if (camera->pitch < -89.0f) camera->pitch = -89.0f;

                    input_changed = true;
                }

                math::Vector3f movement(0.0f, 0.0f, 0.0f);

                if (input_state.move_forward)
                    movement += camera->forward;
                if (input_state.move_backward)
                    movement -= camera->forward;
                if (input_state.move_right)
                    movement += camera->right;
                if (input_state.move_left)
                    movement -= camera->right;
                if (input_state.move_up)
                    movement += camera->up;
                if (input_state.move_down)
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

            void UpdateTransform(CameraComponent* camera) override
            {
                if (!camera)
                    return;
                camera->target = camera->position + camera->forward;
            }
        };

        class ViewModelCameraMode final : public CameraModeProcessor
        {
        public:
            CameraComponent::ControlMode GetMode() const override
            {
                return CameraComponent::ControlMode::ViewModel;
            }

            void ProcessInput(CameraComponent* camera, const CameraInputState& input_state, float /*deltaTime*/) override
            {
                if (!camera)
                    return;

                bool input_changed = false;

                if (input_state.left_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
                {
                    camera->yaw += input_state.mouse_delta.x * camera->rotation_sensitivity * camera->input_invert.x;
                    camera->pitch -= input_state.mouse_delta.y * camera->rotation_sensitivity * camera->input_invert.y;

                    if (camera->pitch > 89.0f) camera->pitch = 89.0f;
                    if (camera->pitch < -89.0f) camera->pitch = -89.0f;

                    input_changed = true;
                }

                if (input_state.wheel_delta != 0)
                {
                    camera->distance *= std::pow(1.0f + camera->zoom_sensitivity, -input_state.wheel_delta);

                    if (camera->distance < camera->min_distance)
                        camera->distance = camera->min_distance;
                    if (camera->distance > camera->max_distance)
                        camera->distance = camera->max_distance;

                    input_changed = true;
                }

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

            void UpdateTransform(CameraComponent* camera) override
            {
                if (!camera)
                    return;
                camera->position = camera->target - camera->forward * camera->distance;
            }
        };

        class LookAtCameraMode final : public CameraModeProcessor
        {
        public:
            CameraComponent::ControlMode GetMode() const override
            {
                return CameraComponent::ControlMode::LookAt;
            }

            void ProcessInput(CameraComponent* camera, const CameraInputState& input_state, float /*deltaTime*/) override
            {
                if (!camera)
                    return;

                bool input_changed = false;

                if (input_state.middle_button && (input_state.mouse_delta.x != 0 || input_state.mouse_delta.y != 0))
                {
                    float pan_speed = 0.01f * camera->distance;
                    math::Vector3f pan_offset =
                        camera->right * (-input_state.mouse_delta.x * pan_speed) +
                        camera->up * (input_state.mouse_delta.y * pan_speed);

                    camera->target += pan_offset;
                    input_changed = true;
                }

                if (input_state.wheel_delta != 0)
                {
                    camera->distance *= std::pow(1.0f + camera->zoom_sensitivity, -input_state.wheel_delta);

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

            void UpdateTransform(CameraComponent* camera) override
            {
                if (!camera)
                    return;
                camera->position = camera->target - camera->forward * camera->distance;
            }
        };

        class FreeCameraMode final : public CameraModeProcessor
        {
        public:
            CameraComponent::ControlMode GetMode() const override
            {
                return CameraComponent::ControlMode::Free;
            }

            void ProcessInput(CameraComponent* /*camera*/, const CameraInputState& /*input_state*/, float /*deltaTime*/) override
            {
            }

            void UpdateTransform(CameraComponent* /*camera*/) override
            {
            }
        };
    }

    CameraSystem::CameraSystem(ECSContext* ctx)
        : input_system(nullptr)
    {
        SetContext(ctx);
        // Set system type and properties
        SetSystemType(SystemType::Camera);
        SetExecutionOrder(ExecutionPhase::TickCamera);

        // Declare dependencies
        AddDependency<InputSystem>();     // Needs input for camera control
        AddDependency<TransformSystem>(); // Needs transforms updated first

        first_person_mode = std::make_unique<FirstPersonCameraMode>();
        view_model_mode = std::make_unique<ViewModelCameraMode>();
        look_at_mode = std::make_unique<LookAtCameraMode>();
        free_mode = std::make_unique<FreeCameraMode>();
    }

    CameraSystem::~CameraSystem()
    {
        Shutdown();
    }

    void CameraSystem::Shutdown()
    {
        if (camera_ubo)
        {
            graph::VkBufferOwner *buf = camera_ubo->ubo();
            delete camera_ubo;
            camera_ubo = nullptr;
            camera_info = nullptr;

            if (camera_ubo_managed && buf)
            {
                graph::BufferManager *buffer_manager = nullptr;
                if (render_context)
                {
                    if (auto *gc = render_context->GetGraphicsContext())
                        buffer_manager = gc->GetBufferManager();
                }
                if (!buffer_manager && context)
                {
                    if (auto *gc = context->GetGraphicsContext())
                        buffer_manager = gc->GetBufferManager();
                }

                if (buffer_manager)
                    buffer_manager->Release(buf);
            }
            camera_ubo_managed = false;
        }
    }

    void CameraSystem::SetRenderContext(graph::RenderContext* ctx)
    {
        if (render_context == ctx)
            return;

        render_context = ctx;
        EnsureCameraResources();
    }

    void CameraSystem::SetViewportInfo(const graph::ViewportInfo* vp)
    {
        viewport_info = vp;
    }

    graph::Camera* CameraSystem::GetCamera()
    {
        return &camera_data;
    }

    const graph::CameraInfo* CameraSystem::GetCameraInfo() const
    {
        return camera_info;
    }

    void CameraSystem::SyncCameraUBO()
    {
        if (!camera_ubo)
            return;

        if (camera_info)
            camera_ubo->Update(*camera_info);

        camera_ubo->MarkDirty();
    }

    void CameraSystem::Update(float deltaTime)
    {
        if (!context)
            return;

        EnsureCameraResources();

        if (!viewport_info)
        {
            auto *rt = context->GetRenderTarget();
            if (rt)
                viewport_info = rt->GetViewportInfo();
        }

        // 获取InputSystem（首次调用时查找）
        if (!input_system)
        {
            input_system = context->GetSystem<InputSystem>().get();
        }

        EnsureInputContext();

        // 收集所有摄像机
        auto cameras = CollectCameras();
        if (cameras.empty())
            return;

        CameraComponent* main_camera = SelectMainCamera(cameras);
        if (main_camera)
            BindCameraResources(main_camera);

        // 收集输入状态
        CollectInput();

        // 处理每个摄像机
        for (auto& camera_comp : cameras)
        {
            if (!camera_comp)
                continue;

            if (first_update_pending)
                camera_comp->matrix_dirty = true;

            // 处理输入
            ProcessInput(camera_comp.get(), deltaTime);

            // 更新局部坐标系
            UpdateBasis(camera_comp.get());

            // 更新位置和目标
            UpdateTransform(camera_comp.get());

            // 更新矩阵
            UpdateMatrices(camera_comp.get());

            // Always commit main camera UBO once per frame to guarantee
            // camera data availability on render-only paths.
            if (camera_comp.get() == main_camera && camera_ubo && camera_comp->camera_info)
            {
                camera_ubo->Update(*camera_comp->camera_info);
                camera_ubo->MarkDirty();
            }

            // 上传到GPU
            UploadToGPU(camera_comp.get());
        }

        if (first_update_pending)
            first_update_pending = false;
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

        const bool mouse_blocked = input_system->IsMouseCaptured() && !input_system->IsMouseCapturedBy(this);

        // 获取动作状态
        input_state.left_button = !mouse_blocked && input_system->IsActionActive(CameraInputMapping::kActionRotate);
        input_state.right_button = !mouse_blocked && input_system->IsActionActive(CameraInputMapping::kActionPanRight);
        input_state.middle_button = !mouse_blocked && input_system->IsActionActive(CameraInputMapping::kActionPanMiddle);

        // 获取滚轮和按键缩放
        float wheel_delta = input_system->GetActionAnalog1D(CameraInputMapping::kActionZoomWheel);
        const float raw_wheel_delta = static_cast<float>(input_system->GetWheelDelta());
        if (wheel_delta == 0.0f)
            wheel_delta = raw_wheel_delta;
        if (input_system->IsActionActive(CameraInputMapping::kActionZoomIn))
            wheel_delta += 1.0f;
        if (input_system->IsActionActive(CameraInputMapping::kActionZoomOut))
            wheel_delta -= 1.0f;
        input_state.wheel_delta = mouse_blocked ? 0.0f : wheel_delta;

        if (mouse_blocked)
            input_state.mouse_delta = math::Vector2i(0, 0);

        //if (wheel_delta != 0.0f || raw_wheel_delta != 0.0f)
        //{
        //    std::cout << "[CameraSystem] Wheel collect action="
        //              << input_system->GetActionAnalog1D(CameraInputMapping::kActionZoomWheel)
        //              << " raw=" << raw_wheel_delta
        //              << " result=" << wheel_delta << "\n";
        //}

        // 获取键盘状态
        input_state.move_forward = input_system->IsActionActive(CameraInputMapping::kActionMoveForward);
        input_state.move_backward = input_system->IsActionActive(CameraInputMapping::kActionMoveBackward);
        input_state.move_left = input_system->IsActionActive(CameraInputMapping::kActionMoveLeft);
        input_state.move_right = input_system->IsActionActive(CameraInputMapping::kActionMoveRight);
        input_state.move_down = input_system->IsActionActive(CameraInputMapping::kActionMoveDown);
        input_state.move_up = input_system->IsActionActive(CameraInputMapping::kActionMoveUp);
    }

    void CameraSystem::EnsureInputContext()
    {
        if (!input_system)
            return;
        input_mapping.EnsureContext(input_system->GetInputMapper());
    }

    void CameraSystem::ProcessInput(CameraComponent* camera, float deltaTime)
    {
        if (!camera)
            return;
        CameraModeProcessor* processor = GetModeProcessor(camera->control_mode);
        if (!processor)
            return;
        processor->ProcessInput(camera, input_state, deltaTime);
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
        CameraModeProcessor* processor = GetModeProcessor(camera->control_mode);
        if (!processor)
            return;
        processor->UpdateTransform(camera);
    }

    CameraModeProcessor* CameraSystem::GetModeProcessor(CameraComponent::ControlMode mode) const
    {
        switch (mode)
        {
            case CameraComponent::ControlMode::FirstPerson:
                return first_person_mode.get();
            case CameraComponent::ControlMode::ViewModel:
                return view_model_mode.get();
            case CameraComponent::ControlMode::LookAt:
                return look_at_mode.get();
            case CameraComponent::ControlMode::Free:
                return free_mode.get();
        }

        return nullptr;
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

    CameraComponent* CameraSystem::SelectMainCamera(const std::vector<std::shared_ptr<CameraComponent>>& cameras) const
    {
        for (const auto& camera : cameras)
        {
            if (camera && camera->is_main_camera)
                return camera.get();
        }

        for (const auto& camera : cameras)
        {
            if (camera)
                return camera.get();
        }

        return nullptr;
    }

    void CameraSystem::BindCameraResources(CameraComponent* camera)
    {
        if (!camera)
            return;

        if (!camera_info && camera_ubo)
            camera_info = camera_ubo->Data();

        if (!viewport_info && camera->viewport_info)
            viewport_info = camera->viewport_info;

        camera->camera_data = &camera_data;
        camera->camera_info = camera_info;
        if (viewport_info)
            camera->viewport_info = viewport_info;
        camera->camera_ubo = camera_ubo;
    }

    void CameraSystem::EnsureCameraResources()
    {
        if (!render_context && context)
            render_context = context->GetRenderContext();

        auto *graphics_context = context ? context->GetGraphicsContext() : nullptr;
        if (!graphics_context && render_context)
            graphics_context = render_context->GetGraphicsContext();

        if (!render_context && !graphics_context)
            return;

        if (!camera_ubo)
        {
            if (graphics_context)
            {
                auto *buffer_manager = graphics_context->GetBufferManager();
                if (buffer_manager)
                {
                    auto *buf = buffer_manager->CreateUBO("CameraUBO", graph::StructuredBufferAccessor<graph::CameraInfo>::GetSize());
                    if (buf)
                    {
                        buf->SetUpdateClass(graph::BufferUpdateClass::CriticalPerFrame);
                        camera_ubo = graph::StructuredBufferAccessor<graph::CameraInfo>::Create(buf, &graph::mtl::SBS_CameraInfo, false);
                    }
                }
            }

            if (camera_ubo)
                camera_ubo_managed = true;
            if (camera_ubo)
                camera_info = camera_ubo->Data();
        }
    }

    // === 数学辅助函数 ===

    math::Vector3f CameraSystem::ComputeForward(float yaw, float pitch)
    {
        // 从欧拉角计算前向向量
        // yaw: 水平旋转角(绕Z轴), pitch: 垂直旋转角(俯仰)
        // Compute forward vector from Euler angles
        // yaw: horizontal rotation angle (around Z-axis), pitch: vertical rotation angle (up/down)
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

