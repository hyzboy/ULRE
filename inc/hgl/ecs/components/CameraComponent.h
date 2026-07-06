#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/math/Vector.h>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<utility>
#include<vector>

namespace hgl::graph
{
    struct Camera;
    struct CameraInfo;
    class ViewportInfo;
    template<typename T> class StructuredBufferAccessor;
}

namespace hgl
{
    namespace ecs
    {
        class TransformComponent;

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

            // === 外部引用 / External references ===
            graph::Camera* camera_data;             ///< 摄像机数据指针 / Camera data pointer
            graph::CameraInfo* camera_info;         ///< 摄像机信息指针 / Camera info pointer
            const graph::ViewportInfo* viewport_info; ///< 视口信息指针 / Viewport info pointer
            graph::StructuredBufferAccessor<graph::CameraInfo>* camera_ubo; ///< 摄像机UBO / Camera uniform buffer object

            // === 标记 / Flags ===
            bool is_main_camera;            ///< 是否为主摄像机 / Is main camera
            bool matrix_dirty;              ///< 矩阵脏标记 / Matrix dirty flag

        public:

            CameraComponent(const std::string& name = "Camera");
            ~CameraComponent() override = default;
        };
    }//namespace ecs
}//namespace hgl


