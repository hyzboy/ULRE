#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/math/Vector.h>
#include<hgl/graph/camera/Camera.h>

namespace hgl::graph
{
    class ViewportInfo;
}

namespace hgl::ecs
{
    /**
     * CameraComponent —— 相机的**参数**（+ 本相机自己的解算输入/结果）。
     *
     * 定稿（doc/world-addresses-and-camera-model-plan.md §2/§3、
     * doc/future/ULRE_FINAL_TARGET_v2_设计约束.md §9「`CameraComponent` 只描述**参数**；
     * 槽位/资源仍归世界级（沿用 16 槽 × 帧槽 + 三级解析）」）：
     *
     * - **相机槽 = 世界资源**：世界私有 `CameraInfoStorage` 持有「16 槽 × 帧槽」的行空间与
     *   **槽账目（槽 → 宿主相机）**，0 号槽恒为本世界默认相机。因此本组件**不持有槽号、
     *   不持有归还挂钩**：任何"这个相机是哪个槽"的问题一律问世界
     *   （`ECSContext::GetCameraSlot(camera)` / `GetCamera(slot)` / `GetDefaultCamera()`），
     *   槽的回收由「宿主生命期 = 弱引用占用期」自动完成（相机销毁 ⇒ 弱引用失效 ⇒ 槽空出）。
     * - 槽语义常量（`kDefaultSlot` / `kSlotCapacity` / `kInvalidSlot`）的唯一真源**仍在此**
     *   （`CameraInfoStorage` 只做别名 + parity 断言），它们是槽的**语义**而非某个相机的状态。
     * - `camera_data` / `camera_info` 是**本相机自己的**解算输入与结果（CPU 权威）；GPU 那张
     *   世界私有的「16 槽 × 帧槽」表是**派生视图**，由 `CameraSystem::PublishCamera` 写入。
     *   （历史上有过同名的裸指针 + `local_` 后缀副本两套名字指同一份真值、且可被外部改指到
     *   世界共享载体；C1-5 / A6 已整删 —— 一份数据只有一个名字。）
     *
     * 所有字段都是public，不包含逻辑方法
     */
    class CameraComponent : public Component
    {
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
        CameraControlMode control_mode; ///< 控制模式（`CameraControlMode`：A7c 提升到命名空间级）

        float distance;                 ///< 距离目标的距离 (ViewModel/LookAt模式) / Distance to target
        float min_distance;             ///< 最小距离 / Minimum distance
        float max_distance;             ///< 最大距离 / Maximum distance

        float rotation_sensitivity;     ///< 旋转灵敏度 / Rotation sensitivity
        float zoom_sensitivity;         ///< 缩放灵敏度 / Zoom sensitivity
        float move_speed;               ///< 移动速度 / Movement speed

        math::Vector2f input_invert;    ///< 输入反转 (x, y) / Input inversion

        // === 本相机的解算输入与结果 / This camera's own solve input & result ===

        /// 解算输入（位置/方向/视锥参数），CPU 权威
        graph::Camera camera_data;

        /// 解算结果（view/projection + 派生量），CPU 权威 —— 世界相机行的来源
        graph::CameraInfo camera_info;

        /// 解算用的视口（**全局**数据：随当前渲染目标而变）。相机不拥有它，
        /// 这里只是"本相机最近一次解算所用的视口"这一绑定。
        const graph::ViewportInfo* viewport_info;

        // === 槽语义（唯一真源；`CameraInfoStorage` 做别名 + parity 断言）===
        // 相机 = **世界相机表里的一个槽 + 一个宿主**（宿主生命期 = 槽占用期），定稿见
        // doc/world-addresses-and-camera-model-plan.md §0.2/§2：
        // - `kDefaultSlot`(0) = 本世界默认相机专属（三级解析的落点，常驻 fallback 也占它）；
        // - 1..`kSlotCapacity`-1 由世界相机表 `AcquireCameraSlot(宿主)` 给出；
        // - **未分配 = `kInvalidSlot`，不是 0**：0 是合法槽号，两者必须区分开
        //   （历史写法用 0 同时表示"默认相机"与"未分配"，导致"没槽的相机"被当成默认相机发出去）。
        static constexpr uint32_t kDefaultSlot  = 0u;
        static constexpr uint32_t kSlotCapacity = 16u;
        static constexpr uint32_t kInvalidSlot  = UINT32_MAX;

        /// 认领该相机槽的**世界**（ECSContext 身份，只做相等比较；不参与渲染）。nullptr = 尚无世界认领。
        /// 用途：pass 覆盖相机（`RenderTo(req.camera)`）必须属于**被渲染的那个世界** ——
        /// 跨世界 = 项目 bug，按 fail-fast 拒绝（否则会把别的世界的相机数据按本世界的行号发出去）。
        /// 槽本身由世界的相机表持有（`CameraInfoStorage::slot_owners`），本字段只是"哪个世界"的显式标记。
        const void* world_owner = nullptr;

        // === 标记 / Flags ===
        bool is_main_camera;            ///< 是否为主摄像机 / Is main camera
        bool matrix_dirty;              ///< 矩阵脏标记 / Matrix dirty flag

        // === 自定义矩阵覆盖（如 CSM / 正交光源相机）===
        bool custom_matrices = false;
        math::Matrix4f custom_view{1.0f};
        math::Matrix4f custom_projection{1.0f};

    public:

        CameraComponent(const std::string& name = "Camera");

        // 不需要析构：相机不持有任何需要归还的世界资源
        // （槽是世界的、按宿主弱引用计占用期；相机销毁 ⇒ 槽自动空出）。
    };
}//namespace hgl::ecs
