#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/CameraComponent.h>
#include<hgl/io/event/InputContext.h>
#include<hgl/math/Vector.h>
#include<vector>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class InputSystem;

        /**
         * CameraSystem - 纯逻辑系统
         * Pure logic system for camera control in ECS architecture
         * 处理所有摄像机的输入、更新和矩阵计算
         */
        class CameraSystem : public System
        {
        private:

            ECSContext* context;
            InputSystem* input_system;

            /// 输入状态缓存 / Input state cache
            struct InputState
            {
                math::Vector2i mouse_pos;           ///< 当前鼠标位置 / Current mouse position
                math::Vector2i mouse_delta;         ///< 鼠标位移 / Mouse delta
                math::Vector2i last_mouse_pos;      ///< 上一帧鼠标位置 / Last frame mouse position

                bool left_button;                   ///< 左键按下 / Left button pressed
                bool right_button;                  ///< 右键按下 / Right button pressed
                bool middle_button;                 ///< 中键按下 / Middle button pressed

                float wheel_delta;                  ///< 滚轮增量 / Wheel delta

                bool key_w, key_s, key_a, key_d;   ///< WASD键 / WASD keys
                bool key_q, key_e;                  ///< QE键 / QE keys

                InputState()
                    : mouse_pos(0, 0)
                    , mouse_delta(0, 0)
                    , last_mouse_pos(0, 0)
                    , left_button(false)
                    , right_button(false)
                    , middle_button(false)
                    , wheel_delta(0.0f)
                    , key_w(false), key_s(false), key_a(false), key_d(false)
                    , key_q(false), key_e(false)
                {
                }
            };

            InputState input_state;

            io::InputContext input_context;
            bool input_context_ready;

        public:

            CameraSystem(ECSContext* ctx);
            ~CameraSystem() override = default;

            void Update(float deltaTime) override;

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
            void UploadToGPU(CameraComponent* camera);

            // === 不同控制模式的输入处理 / Input processing for different control modes ===

            /// 第一人称输入处理 / First-person input processing
            void ProcessFirstPersonInput(CameraComponent* camera, float deltaTime);

            /// 视图模型输入处理 / View-model input processing
            void ProcessViewModelInput(CameraComponent* camera, float deltaTime);

            /// 观察模式输入处理 / Look-at input processing
            void ProcessLookAtInput(CameraComponent* camera, float deltaTime);

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
