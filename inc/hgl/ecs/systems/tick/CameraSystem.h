#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraInputMapping.h>
#include<hgl/math/Vector.h>
#include<hgl/graph/camera/Camera.h>
#include<vector>
#include<memory>

namespace hgl::graph
{
    struct Camera;
    struct CameraInfo;
    class ViewportInfo;
    class RenderContext;
    class GlobalSSBOBufferRegistry;
    template<typename T> class StructView;
}

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class InputSystem;

        struct CameraInputState
        {
            math::Vector2i mouse_pos;           ///< 当前鼠标位置 / Current mouse position
            math::Vector2i mouse_delta;         ///< 鼠标位移 / Mouse delta
            math::Vector2i last_mouse_pos;      ///< 上一帧鼠标位置 / Last frame mouse position

            bool left_button;                   ///< 左键按下 / Left button pressed
            bool right_button;                  ///< 右键按下 / Right button pressed
            bool middle_button;                 ///< 中键按下 / Middle button pressed

            float wheel_delta;                  ///< 滚轮增量 / Wheel delta

            bool move_forward;                 ///< 前进 / Move forward
            bool move_backward;                ///< 后退 / Move backward
            bool move_left;                    ///< 左移 / Move left
            bool move_right;                   ///< 右移 / Move right
            bool move_down;                    ///< 下移 / Move down
            bool move_up;                      ///< 上移 / Move up

            CameraInputState()
                : mouse_pos(0, 0)
                , mouse_delta(0, 0)
                , last_mouse_pos(0, 0)
                , left_button(false)
                , right_button(false)
                , middle_button(false)
                , wheel_delta(0.0f)
                , move_forward(false)
                , move_backward(false)
                , move_left(false)
                , move_right(false)
                , move_down(false)
                , move_up(false)
            {
            }
        };

        class CameraModeProcessor
        {
        public:
            virtual ~CameraModeProcessor() = default;
            virtual CameraControlMode GetMode() const = 0;
            virtual void ProcessInput(CameraComponent* camera, const CameraInputState& input_state, float deltaTime) = 0;
            virtual void UpdateTransform(CameraComponent* camera) = 0;
        };

        /**
         * CameraSystem - 纯逻辑系统
         * Pure logic system for camera control in ECS architecture
         * 处理所有摄像机的输入、更新和矩阵计算
         */
        class CameraSystem : public System
        {
        private:

            InputSystem* input_system;

            CameraInputState input_state;

            std::unique_ptr<CameraModeProcessor> first_person_mode;
            std::unique_ptr<CameraModeProcessor> view_model_mode;
            std::unique_ptr<CameraModeProcessor> look_at_mode;
            std::unique_ptr<CameraModeProcessor> free_mode;

            CameraInputMapping input_mapping;

            graph::RenderContext* render_context = nullptr;
            const graph::ViewportInfo* viewport_info = nullptr;
            bool first_update_pending = true;
            uint cached_viewport_width = 0;
            uint cached_viewport_height = 0;

            /// PublishCamera 遇到"相机没认领到槽"时的一次性告警闸（逐帧刷屏没有意义）
            bool warned_publish_without_slot = false;

            /// pass 级相机覆盖（RenderTo(request.camera) 期间非空）：
            /// Update 只处理该相机并强制重算（它的世界相机槽也从这条路径认领）；
            /// pass 结束由 RenderTo → `RestoreMainCamera()` 恢复主相机
            CameraComponent* override_camera = nullptr;

        public:

            CameraSystem(ECSContext* ctx = nullptr);
            ~CameraSystem() override = default;

            void Update(float deltaTime) override;

            /// 发布单个相机在**指定帧槽**的 CameraInfo 行（行号 = 槽号 × 帧槽总数 + 帧槽，
            /// 槽号问世界：`ECSContext::GetCameraSlot(camera)`）。
            ///
            /// 为什么除了 PublishCameraRows 还需要本接口：阴影光源相机是**系统内建相机**
            /// （`EnvironmentSystem` 用 make_shared 创建，**不经 Entity/AddComponent 注册**）
            /// ⇒ 不在 component_registry 里，`CollectCameras()` 看不到它，必须由 RenderTo 按
            /// `req.camera` 直接调用。（历史注释曾误记为"相机属另一个世界"；CSM 始终是
            /// "一个世界 + 多个渲染过滤程"，见 doc/world-addresses-and-camera-model-plan.md §7。）
            void PublishCamera(const CameraComponent* camera, uint32_t frame_slot);

            /// 发布本世界全部相机（由 ECSContext::PrepareRenderPassSetup 调用）。
            ///
            /// 不能在 tick 阶段发布：本帧数据槽要等 acquire（主帧）/ 进入离屏 pass
            /// （RT 槽带）之后才确定，tick 时拿到的是上一帧的槽。
            void PublishCameraRows(uint32_t frame_slot);

            void SetRenderContext(graph::RenderContext* ctx);
            void SetViewportInfo(const graph::ViewportInfo* vp);

            /// pass 级相机覆盖（RenderTo 期间由 ECSContext 设置/解除）
            void SetOverrideCamera(CameraComponent* camera) { override_camera = camera; }
            CameraComponent* GetOverrideCamera() const { return override_camera; }

            /// pass 覆盖解除后恢复主相机共享数据（不重算矩阵、不消耗输入）
            void RestoreMainCamera();

            /// 强制下一次 Update 重算当前选中相机（主相机）的矩阵——
            /// pass 覆盖解除后恢复共享数据用（主相机可能不脏，否则
            /// 共享 camera_info 会残留 pass 相机的矩阵）
            void ForceRefreshSelectedCamera();
            void MarkAllCameraMatricesDirty();

            const graph::ViewportInfo* GetViewportInfo() const { return viewport_info; }

            /// 获取当前场景中激活的主相机组件（= 三级解析结果，同时认领 0 号槽）
            CameraComponent *GetMainCameraComponent();

            /// 本 pass 生效相机：pass 覆盖相机（RenderTo(req.camera) 期间）优先，否则本世界主相机。
            /// 相机数据一律以**组件自己的** CameraInfo 为准（世界共享载体已退役）。
            CameraComponent* GetActiveCameraComponent();

            /// 本 pass 生效相机的 CameraInfo（= 组件自己的 camera_info；无相机时为 nullptr）
            const graph::CameraInfo* GetActiveCameraInfo();

        private:

            /// 收集所有摄像机组件 / Collect all camera components
            std::vector<std::shared_ptr<CameraComponent>> CollectCameras();

            /// 收集输入状态 / Collect input state
            void CollectInput();

            /// 初始化输入映射上下文 / Ensure input context is setup
            void EnsureInputContext();
            /// 处理输入 / Process input
            void ProcessInput(CameraComponent* camera, float deltaTime);

            /// 更新局部坐标系 / Update local basis vectors
            void UpdateBasis(CameraComponent* camera);

            /// 更新位置和目标 / Update position and target
            void UpdateTransform(CameraComponent* camera);

            /// 更新矩阵 / Update matrices
            void UpdateMatrices(CameraComponent* camera);

            /// 上传到GPU / Upload to GPU

            CameraModeProcessor* GetModeProcessor(CameraControlMode mode) const;

            /// 三级解析出本 pass 要用的相机（`doc/world-addresses-and-camera-model-plan.md` §3）：
            /// ① 0 号槽的默认相机（含常驻 fallback）→ ② 显式 `is_main_camera` → ③ 最小 `EntityID` 的相机
            /// → ④ 都没有则生成常驻 fallback（`(0,0,0)`、占 0 号槽）。**渲染必有一个相机**。
            CameraComponent* SelectMainCamera(const std::vector<std::shared_ptr<CameraComponent>>& cameras);

            /// 让某相机认领 0 号槽（= 本世界默认相机），并记进世界（下一帧解析的第 ① 级）
            CameraComponent* ClaimDefaultCamera(const std::shared_ptr<CameraComponent>& camera);

            /// 绑本相机的 viewport + **认领世界相机槽**（`ECSContext::EnsureCameraSlot`，
            /// **只在本帧 tick / pass 覆盖上下文调用**）。
            /// `is_default` = 本 pass 的默认相机（占 0 号槽）；
            /// 其它相机（灯光 / 镜子 / 系统内建）从世界相机表按需分配 1..15 槽。
            void BindCameraResources(CameraComponent* camera, bool is_default = false);

            // === 数学辅助函数 / Math helper functions ===

            /// 从欧拉角计算前向向量 / Compute forward vector from euler angles
            static math::Vector3f ComputeForward(float yaw, float pitch);

            /// 计算右向和上向向量 / Compute right and up vectors
            static void ComputeRightUp(const math::Vector3f& forward,
                                      const math::Vector3f& world_up,
                                      math::Vector3f& right,
                                      math::Vector3f& up);
        };
    }//namespace ecs
}//namespace hgl

