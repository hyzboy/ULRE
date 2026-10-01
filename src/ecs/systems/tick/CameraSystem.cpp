#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/InputSystem.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/ecs/support/CameraInfoStorage.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>
#include<cmath>
#include<iostream>
#include<algorithm>

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
        SetExecutionPhase(ExecutionPhase::TickCamera);

        // Declare dependencies

        first_person_mode = std::make_unique<FirstPersonCameraMode>();
        view_model_mode = std::make_unique<ViewModelCameraMode>();
        look_at_mode = std::make_unique<LookAtCameraMode>();
        free_mode = std::make_unique<FreeCameraMode>();
    }

    void CameraSystem::SetRenderContext(graph::RenderContext* ctx)
    {
        if (render_context == ctx)
            return;

        render_context = ctx;
    }

    void CameraSystem::RestoreMainCamera()
    {
        override_camera = nullptr;

        auto cameras = CollectCameras();
        CameraComponent* main_cam = SelectMainCamera(cameras);
        if (!main_cam)
            return;

        // 覆盖相机（离屏 pass）在 UpdateMatrices 里写的是**它自己组件**的 CameraInfo
        // （C1-5 后不再有世界共享载体）；但 pass 期间 viewport 会切到离屏 RT
        // （SetViewportInfo 会标脏全部相机），所以 pass 结束后仍要**重新解算主相机**，
        // 保证主帧的相机行（PublishCameraRows 从 camera->camera_info 取值）是按主 RT 的
        // viewport 解出的——只做 struct 拷贝/指针切回是不够的。
        main_cam->matrix_dirty = true;
        UpdateMatrices(main_cam);
    }

    void CameraSystem::ForceRefreshSelectedCamera()
    {
        auto cameras = CollectCameras();
        CameraComponent* selected = override_camera ? override_camera : SelectMainCamera(cameras);
        if (selected)
            selected->matrix_dirty = true;
    }

    void CameraSystem::SetViewportInfo(const graph::ViewportInfo* vp)
    {
        uint new_w = vp ? vp->GetViewportWidth() : 0;
        uint new_h = vp ? vp->GetViewportHeight() : 0;

        const bool viewport_changed = (viewport_info != vp) || (cached_viewport_width != new_w) || (cached_viewport_height != new_h);
        viewport_info = vp;
        cached_viewport_width = new_w;
        cached_viewport_height = new_h;

        if (viewport_changed)
            MarkAllCameraMatricesDirty();
    }

    CameraComponent* CameraSystem::GetActiveCameraComponent()
    {
        // pass 覆盖相机优先（离屏 pass 期间"本 pass 生效相机"就是它），否则走**本世界主相机的
        // 三级解析**（复用 GetMainCameraComponent ⇒ 与 Update/PublishCameraRows 同一套解析与
        // 幂等认领：① 已有默认相机 ② is_main_camera ③ 最小 EntityID ④ 常驻 fallback）。
        if (override_camera)
            return override_camera;

        return GetMainCameraComponent();
    }

    const graph::CameraInfo* CameraSystem::GetActiveCameraInfo()
    {
        CameraComponent* camera = GetActiveCameraComponent();
        return camera ? &camera->camera_info : nullptr;
    }

    void CameraSystem::Update(float deltaTime)
    {
        if (!context)
            return;

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

        // pass 级相机覆盖（RenderTo(request.camera) 期间）：只处理覆盖相机，
        // 强制重算——独立解算覆盖相机矩阵并写入其独立的全局 SSBO 行（A6 起不再有 UBO 第二写入路径）
        if (override_camera)
        {
            // 契约（doc/world-addresses-and-camera-model-plan.md §6.6②）：pass 相机必须属于**被渲染的世界**。
            // 跨世界相机 = 项目 bug ⇒ fail-fast 拒绝（它的槽号是别的世界的行空间，发出去就是错数据）。
            if (override_camera->world_owner && override_camera->world_owner != context)
            {
                GLogError("[CameraSystem] pass 相机 \"%s\" 属于另一个世界（world_owner=%p，本世界=%p）——拒绝使用",
                          override_camera->GetName().c_str(), override_camera->world_owner, static_cast<const void*>(context));
                return;
            }

            BindCameraResources(override_camera, override_camera->is_main_camera);
            override_camera->matrix_dirty = true;

            UpdateBasis(override_camera);
            UpdateTransform(override_camera);
            UpdateMatrices(override_camera);
            return;
        }

        EnsureInputContext();

        // 收集所有摄像机
        auto cameras = CollectCameras();

        // 三级解析：本世界**必有**相机（必要时是常驻 fallback 相机，见 SelectMainCamera ①..④）
        CameraComponent* main_camera = SelectMainCamera(cameras);

        // 解析结果可能是**不在组件表**里的常驻 fallback（世界内一个相机组件都没有）：
        // 它同样要绑槽 / 解算 / 发布，否则主帧没有合法相机数据。
        std::vector<CameraComponent *> render_cameras;
        render_cameras.reserve(cameras.size() + 1u);
        for (auto& camera_comp : cameras)
            if (camera_comp)
                render_cameras.push_back(camera_comp.get());
        if (main_camera && std::find(render_cameras.begin(), render_cameras.end(), main_camera) == render_cameras.end())
            render_cameras.push_back(main_camera);

        // 收集输入状态
        CollectInput();

        // 处理每个摄像机
        for (auto* camera_comp : render_cameras)
        {
            if (!camera_comp)
                continue;

            if (first_update_pending)
                camera_comp->matrix_dirty = true;

            const bool is_main = (camera_comp == main_camera);

            BindCameraResources(camera_comp, is_main);

            // 只有主相机响应玩家输入，从属相机跳过输入；常驻 fallback 不是实体相机，不吃输入
            if (is_main && !context->IsFallbackCamera(camera_comp))
                ProcessInput(camera_comp, deltaTime);

            // 更新局部坐标系
            UpdateBasis(camera_comp);

            // 更新位置和目标
            UpdateTransform(camera_comp);

            // 每个相机各自解算并写入自己的 SSBO 行
            UpdateMatrices(camera_comp);
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

        // 鼠标被其他消费者（如 Gizmo 拖拽）独占时，相机不响应输入
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

        // 更新本相机的解算输入：`CameraComponent::camera_data` 是本相机**自己的**权威数据
        // （A6 起不再有"可指向世界共享载体"的裸指针 ⇒ 没有可缺失的指针、也没有第二份真值）
        camera->camera_data.pos = camera->position;
        camera->camera_data.viewDirection = camera->forward;
        camera->camera_data.world_up = camera->world_up;
        camera->camera_data.fovY = camera->fov;
        camera->camera_data.znear = camera->near_plane;
        camera->camera_data.zfar = camera->far_plane;

        // Camera-Relative Rendering: 同步 double 精度世界坐标
        camera->camera_data.world_position_double = math::Vector3d(
            static_cast<double>(camera->position.x),
            static_cast<double>(camera->position.y),
            static_cast<double>(camera->position.z));

        // 更新camera_info
        {
            if (camera->custom_matrices)
            {
                camera->camera_info.view               = camera->custom_view;
                camera->camera_info.projection         = camera->custom_projection;

                // 派生量与非矩阵字段一律委托给与主相机路径同一份实现，禁止在本分支
                // 另写一份：view_line / camera_world_pos 是阴影级联选级的输入
                // （ShaderLibrary/shadow/pcf_shadow.glsl EvalPCFShadow），
                // camera_facing_* 是 billboard 的输入（orient_camera_facing.glsl），
                // 两条路径曾各自实现而漂移。
                graph::RefreshCameraInfoDerived(&camera->camera_info);

                graph::RefreshCameraInfoCamera(&camera->camera_info, &camera->camera_data);
            }
            else if (camera->viewport_info)
            {
                // 视图矩阵一律由**组件权威前向 `forward`** 构造，**不要用 `target`**：
                // `target` 由 UpdateTransform 维护；当 position 被外部直接写（示例的 autowalk
                // `position.x += speed*delta`，以及任何在 tick 之后/之外写位置的路径）时，它会慢一拍。
                // `LookAtMatrix(position, 慢一拍的 target, up)` 会算出方向差约 30° 的视图 ⇒ **该帧整幅
                // 画面渲成另一个视角**（实测：同一姿态下 viewT 从 ~1.2m 跳到 ~13.5m，画面看起来像
                // "回到几秒前的位置/另一个机位"）。
                // `forward` 与 `camera_data->viewDirection`、shader 的 view_line 同源（UpdateBasis 由
                // yaw/pitch 得出），与 position 永远同帧一致；LookAt 模式的方向仍来自 target ⇒ 等价。
                camera->camera_info.view = math::LookAtMatrix(
                    camera->position,
                    camera->position + camera->forward,
                    camera->world_up
                );

                // 调用RefreshCameraInfo更新所有矩阵
                graph::RefreshCameraInfo(
                    &camera->camera_info,
                    camera->viewport_info,
                    &camera->camera_data
                );
            }
        }

        // CameraInfo 不再在此写入：本帧数据槽（= 当前 RT 的槽）要等 acquire / 进入离屏 pass
        // 之后才确定，tick 阶段写会落到上一帧的槽、主帧读到上一帧的相机数据。
        // 改由 PublishCameraRows() / PublishCamera() 在 PrepareRenderPassSetup 与 RenderTo 中发布。

        camera->matrix_dirty = false;
    }


    void CameraSystem::PublishCamera(const CameraComponent *camera,const uint32_t frame_slot)
    {
        if (!camera)
            return;

        // 槽号问**世界**（相机槽是世界资源，组件不再自持槽号）
        const uint32_t camera_slot = context ? context->GetCameraSlot(camera) : CameraComponent::kInvalidSlot;

        // 槽未认领（未分配 / 槽耗尽）：**不发布**——按未分配哨兵算出的行号会越界（甚至落进别的世界）。
        // 不静默：一次性告警（每帧刷屏没意义，判据是"这个相机根本没进 GPU"）。
        if (!CameraInfoStorage::IsValidSlot(camera_slot))
        {
            if (!warned_publish_without_slot)
            {
                warned_publish_without_slot = true;
                GLogError("[CameraSystem] PublishCamera 跳过未认领相机槽的相机 \"%s\"："
                          "本世界相机槽上限 %u，检查是否有相机未归还 / 行池未就绪",
                          camera->GetName().c_str(), CameraInfoStorage::kSlotCapacity);
            }
            return;
        }

        auto *storage = context ? context->GetCameraInfoStorage() : nullptr;
        if (!storage || !storage->IsReady())
            return;

        // 行号 = 相机槽 * 帧槽总数 + frame_slot：主帧槽 [0,4) 与离屏 RT 槽带 [4,8) 不相交，
        // 离屏 prepass 写光源相机不会踩到主帧在途的那一份。行号越界由存储报错。
        storage->WriteCameraRow(camera_slot, frame_slot, camera->camera_info);
    }

    void CameraSystem::PublishCameraRows(const uint32_t frame_slot)
    {
        auto cameras = CollectCameras();

        // 本函数由 PrepareRenderPassSetup 调用，**可能早于本帧的 CameraSystem::Update**——
        // 第一帧就是这种情况（相机组件已在，但还没有世界相机槽）。所以发布前先解析默认相机 +
        // 给每台相机认领槽；两步都幂等（默认相机已定则不变、已认领的相机不动）。
        // 不做这一步的后果：相机的槽是"未分配"⇒ 发布被跳过 ⇒ 主帧读到的 0 号行是空的。
        CameraComponent *default_camera = SelectMainCamera(cameras);

        // 只认领**槽**（不碰 viewport —— 本函数也会在离屏 pass 的设置阶段被调用，
        // 那时 viewport_info 是离屏 RT 的，绑上去会污染主帧投影）。
        for (auto &camera : cameras)
            context->EnsureCameraSlot(camera.get(), camera.get() == default_camera);

        if (default_camera)
            context->EnsureCameraSlot(default_camera, true);

        for (auto &camera : cameras)
            PublishCamera(camera.get(), frame_slot);

        // 默认相机（0 号槽）可能不在组件表里（常驻 fallback 相机 / 系统内建相机）⇒ 单独补发，
        // 否则主帧读到的 0 号行是空的（着色器把整个场景算成退化相机）。
        if (default_camera)
        {
            const bool in_registry = std::any_of(cameras.begin(), cameras.end(),
                [default_camera](const std::shared_ptr<CameraComponent>& camera)
                {
                    return camera.get() == default_camera;
                });

            if (!in_registry)
                PublishCamera(default_camera, frame_slot);
        }
    }


    CameraComponent* CameraSystem::SelectMainCamera(const std::vector<std::shared_ptr<CameraComponent>>& cameras)
    {
        // 三级解析（doc/world-addresses-and-camera-model-plan.md §3）：渲染**必须**有一个相机。
        if (!context)
            return nullptr;

        // ① 0 号槽的默认相机仍在 ⇒ 沿用：跨帧稳定，不因集合顺序/新增相机而抖动换相机。
        if (auto *def = context->GetDefaultCamera())
        {
            for (const auto &camera : cameras)
            {
                if (camera.get() == def)
                    return def;
            }
        }

        // ② 显式指定的主相机（示例搭建期 `camera->is_main_camera = true`）
        for (const auto& camera : cameras)
        {
            if (camera && camera->is_main_camera)
                return ClaimDefaultCamera(camera);
        }

        // ③ 已加载实体中 **EntityID 最小**的相机（同实体多相机按组件注册顺序 ⇒ 集合序即可）
        std::shared_ptr<CameraComponent> best;
        for (const auto& camera : cameras)
        {
            if (!camera)
                continue;

            if (!best || camera->GetOwnerID() < best->GetOwnerID())
                best = camera;
        }

        if (best)
            return ClaimDefaultCamera(best);

        // ④ 世界内一个相机组件都没有 ⇒ 强制生成常驻 fallback（(0,0,0)、占 0 号槽）
        return context->EnsureFallbackCamera();
    }

    CameraComponent* CameraSystem::ClaimDefaultCamera(const std::shared_ptr<CameraComponent>& camera)
    {
        // 选中的相机**成为 0 号槽的宿主**（= 本世界默认相机）：下一帧三级解析的第 ① 级就是它，
        // 相机集合的顺序变化 / 新增相机都不会把主相机换掉。
        // 0 号槽唯一 ⇒ 原宿主（含常驻 fallback）自动让位，不需要在这里显式清谁的槽
        // （历史写法要手工把 fallback 的槽号抹成"未分配"——那正是"两份槽真值"的产物）。
        if (camera && context)
            context->SetDefaultCamera(camera);

        return camera.get();
    }

    CameraComponent* CameraSystem::GetMainCameraComponent()
    {
        auto cameras = CollectCameras();
        return SelectMainCamera(cameras);
    }

    void CameraSystem::BindCameraResources(CameraComponent* camera, bool is_default)
    {
        if (!camera)
            return;

        if (!viewport_info && camera->viewport_info)
            viewport_info = camera->viewport_info;

        // ⚠ viewport 绑定只能在**本帧的 tick/覆盖上下文**里做（Update）：`viewport_info` 是"当前 RT 的
        // viewport"，离屏 pass 的设置阶段它是**离屏 RT** 的。若在发布路径（PublishCameraRows，离屏 pass
        // 也会走）里绑，主相机会拿到离屏 RT 的 viewport ⇒ 主帧投影/级联尺寸全错
        // （实测症状：ATS 的 D1 bbox 112x58 → 146x77、D3 受影像素 18189 → 30766）。
        if (viewport_info && !camera->viewport_info)
            camera->viewport_info = viewport_info;

        // 相机槽是**世界**资源（16 槽 × 帧槽、0 号槽恒为默认相机）：认领/升格一律走世界访问器
        context->EnsureCameraSlot(camera, is_default);
    }

    void CameraSystem::MarkAllCameraMatricesDirty()
    {
        if (!context)
            return;

        std::vector<std::shared_ptr<CameraComponent>> cameras;
        context->GetComponents<CameraComponent>(cameras);

        for (const auto& camera : cameras)
        {
            if (camera)
                camera->matrix_dirty = true;
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
