#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/math/Vector.h>
#include<hgl/graph/camera/Camera.h>
#include<functional>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<utility>
#include<vector>

namespace hgl::graph
{
    class ViewportInfo;
}

namespace hgl
{
    namespace ecs
    {
        /**
         * CameraComponent - 纯数据组件
         * Pure data component for camera in ECS architecture
         * 所有字段都是public，不包含逻辑方法
         */
        class CameraComponent : public Component
        {
        public:

            /// 控制模式枚举 / Control mode enum
            enum class ControlMode
            {
                FirstPerson,    ///< 第一人称模式 (WASD移动 + 鼠标旋转)
                ViewModel,      ///< 视图模型模式 (左键旋转 + 滚轮缩放 + 右键平移)
                LookAt,         ///< 观察模式 (中键平移 + 滚轮距离)
                Free            ///< 自由模式
            };

        public:

            // === 基础摄像机数据 / Basic camera data ===
            math::Vector3f position;        ///< 摄像机位置 / Camera position
            math::Vector3f target;          ///< 目标点 / Target point
            math::Vector3f world_up;        ///< 世界向上向量 / World up vector

            float fov;                      ///< 视场角 / Field of view (degrees)
            float near_plane;               ///< 近平面 / Near clipping plane
            float far_plane;                ///< 远平面 / Far clipping plane

            // === 欧拉角 / Euler angles ===
            float yaw;                      ///< 偏航角 / Yaw angle (degrees)
            float pitch;                    ///< 俯仰角 / Pitch angle (degrees)
            float roll;                     ///< 翻滚角 / Roll angle (degrees)

            // === 局部坐标系 / Local coordinate system ===
            math::Vector3f forward;         ///< 前向向量 / Forward vector
            math::Vector3f right;           ///< 右向向量 / Right vector
            math::Vector3f up;              ///< 上向向量 / Up vector

            // === 控制参数 / Control parameters ===
            ControlMode control_mode;       ///< 控制模式 / Control mode

            float distance;                 ///< 距离目标的距离 (ViewModel/LookAt模式) / Distance to target
            float min_distance;             ///< 最小距离 / Minimum distance
            float max_distance;             ///< 最大距离 / Maximum distance

            float rotation_sensitivity;     ///< 旋转灵敏度 / Rotation sensitivity
            float zoom_sensitivity;         ///< 缩放灵敏度 / Zoom sensitivity
            float move_speed;               ///< 移动速度 / Movement speed

            math::Vector2f input_invert;    ///< 输入反转 (x, y) / Input inversion

            // === 独立数据与外部引用 / Local buffers and references ===
            graph::Camera local_camera_data{};
            graph::CameraInfo local_camera_info{};

            graph::Camera* camera_data;             ///< 摄像机数据指针 / Camera data pointer
            graph::CameraInfo* camera_info;         ///< 摄像机信息指针 / Camera info pointer
            const graph::ViewportInfo* viewport_info; ///< 视口信息指针 / Viewport info pointer
            // === 相机槽（世界内）/ World-local camera slot ===
            // 相机 = **世界相机存储里的一个槽 + 一个拥有者**（普通相机 / 灯光相机 / 镜子相机 /
            // 系统内建相机），定稿见 doc/world-addresses-and-camera-model-plan.md §2/§3：
            // - `kDefaultSlot`(0) = 本世界默认相机专属（三级解析的落点，常驻 fallback 也占它）；
            // - 1..`kSlotCapacity`-1 由世界存储 `CameraInfoStorage::AcquireCameraSlot()` 给出，
            //   拥有者负责归还；容量 16、超限 fail-fast、不扩容；
            // - **未分配 = `kInvalidSlot`，不是 0**：0 是合法槽号，两者必须区分开
            //   （历史写法用 0 同时表示"默认相机"与"未分配"，导致"没槽的相机"被当成默认相机发出去）。
            static constexpr uint32_t kDefaultSlot  = 0u;
            static constexpr uint32_t kSlotCapacity = 16u;
            static constexpr uint32_t kInvalidSlot  = UINT32_MAX;

            /// 是否已在本世界相机存储里认领到槽（0 号槽 = 默认相机，也算）
            bool HasCameraSlot() const { return camera_id < kSlotCapacity; }

            /// 认领该相机槽的**世界**（ECSContext 身份，只做相等比较；不参与渲染）。
            /// 用途：pass 覆盖相机（`RenderTo(req.camera)`）必须属于**被渲染的那个世界** ——
            /// 跨世界 = 项目 bug，按 fail-fast 拒绝（否则会把别的世界的相机数据按本世界的行号发出去）。
            const void* world_owner = nullptr;

            uint32_t camera_id = kInvalidSlot;      ///< 相机槽号（世界内；kDefaultSlot = 本世界默认相机，kInvalidSlot = 未分配）

            /// 归还相机槽的回调：`CameraSystem::EnsureCameraSlot` 在认领槽时挂上（捕获本世界存储的
            /// **弱引用**，`weak_ptr<CameraInfoStorage>`），**组件析构即自动归还** —— 否则相机的
            /// 反复创建/销毁会把 16 个槽漏空。
            /// 用回调而不是 ECSContext 指针：组件只做数据 + 生命周期，不该依赖上下文类型；用弱引用
            /// 而不是裸指针：相机可能比世界活得久（作者/示例持有 shared_ptr），世界先销毁时
            /// `lock()` 失败 ⇒ 挂钩自动失效，绝不触碰已释放的存储（**不要**回到「世界销毁时统一摘钩」
            /// 的注册表方案：世界从未 Initialize 时 `Shutdown` 走早退分支，摘不到 ⇒ 悬垂指针/段错误）。
            std::function<void(uint32_t slot)> slot_releaser;

            // === 标记 / Flags ===
            bool is_main_camera;            ///< 是否为主摄像机 / Is main camera
            bool matrix_dirty;              ///< 矩阵脏标记 / Matrix dirty flag

            // === 自定义矩阵覆盖（如 CSM / 正交光源相机）===
            bool custom_matrices = false;
            math::Matrix4f custom_view{1.0f};
            math::Matrix4f custom_projection{1.0f};

        public:

            CameraComponent(const std::string& name = "Camera");
            ~CameraComponent() override;    ///< 归还相机槽（有挂钩且持非 0 号槽时）
        };
    }//namespace ecs
}//namespace hgl


