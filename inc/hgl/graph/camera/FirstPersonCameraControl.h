#pragma once

/**
 * thank for LearnOpenGL
 * link: https://learnopengl.com/Getting-started/Camera
 */

#include<hgl/graph/camera/CameraControl.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/Time.h>
#include <cmath>

namespace hgl::graph
{
    /**
     * CN: 第一人称摄像机控制类
     * EN: First-person camera controller
     */
    class FirstPersonCameraControl:public CameraControl
    {
        // CN: 欧拉角（以弧度存储）
        // EN: Euler angles (stored in radians)
        float pitch;        ///< CN: 绕X轴旋转（俯仰） EN: rotation about the X axis (elevation)
        float yaw;          ///< CN: 绕Z轴旋转（方位，Z轴向上） EN: rotation about the Z axis (azimuth, Z is up)
        float roll;         ///< CN: 绕Y轴旋转（横滚） EN: rotation about the Y axis (bank)

        // CN: 相机局部基向量
        // EN: Local camera basis vectors
        Vector3f forward;   ///< CN: 前向（归一化） EN: forward direction (normalized)
        Vector3f right;     ///< CN: 右向（归一化） EN: right direction (normalized)
        Vector3f up;        ///< CN: 上向（归一化） EN: up direction (normalized)

        // CN: 摄像机位置到目标点的距离（标量）
        // EN: Distance from camera position to look target along the forward vector
        float distance_to_target;          ///< CN: 沿前向的距离 EN: distance from camera to target along forward vector

        Vector3f target;            ///< CN: 世界空间中的目标点 EN: target point in world space

        // CN: 输入轴反转标志（+1 或 -1）
        // EN: Axis inversion signs: +1 or -1 per axis to invert mouse axes
        Vector2f input_invert_sign;       ///< CN: x/y 反转符号（1 或 -1） EN: x/y inversion sign for input (1 or -1)

        // CN: 旋转灵敏度（以每像素度为单位）
        // EN: Rotation sensitivity expressed in degrees per pixel of mouse movement
        float rotation_sensitivity_deg_per_pixel;  ///< CN: 度/像素 EN: degrees per pixel

    public:

        static CameraControlIDName StaticControlName()
        {
            static CameraControlIDName FPCC("FirstPersonCameraControl");

            return FPCC;
        }

        const CameraControlIDName GetControlName() const override 
        {
            return StaticControlName();
        }

    public:

        // CN: 简单的设置结构，便于UI/配置系统读取/写入
        // EN: Simple settings struct to facilitate UI/config system read/write
        struct Settings
        {
            float rotation_sensitivity_deg_per_pixel; // degrees per pixel
            bool invert_x;
            bool invert_y;
        };

        FirstPersonCameraControl(ViewportInfo *v,Camera *c,UBOCameraInfo *ci):CameraControl(v,c,ci)
        {
            target=Vector3f(0.0f);
            up=Vector3f(0,0,1);
            distance_to_target=0.0f;

            // initialize orientation (radians)
            pitch=0;
            yaw  =deg2rad(-90.0f);
            roll =0;

            input_invert_sign.x=-1;
            input_invert_sign.y=1;

            // default: ~0.2 degrees per pixel (previous default was 0.002 radians/pixel)
            rotation_sensitivity_deg_per_pixel = 0.2f; // degrees per pixel

            UpdateCameraVector();
        }
        virtual ~FirstPersonCameraControl()=default;
            
        // CN: 配置输入轴是否反转（传 true 则反转）
        // EN: Configure axis inversion for input: pass true to invert that axis
        void SetInvertAxis(bool invert_x,bool invert_y)
        {
            input_invert_sign.x = invert_x ? -1.0f : 1.0f;
            input_invert_sign.y = invert_y ? -1.0f : 1.0f;
        }

        // CN: 获取当前轴反转设置
        // EN: Get current axis inversion settings
        void GetInvertAxis(bool &out_invert_x,bool &out_invert_y)const
        {
            out_invert_x = (input_invert_sign.x < 0.0f);
            out_invert_y = (input_invert_sign.y < 0.0f);
        }

        // CN: 设置旋转灵敏度（度/像素）
        // EN: Set rotation sensitivity (degrees per pixel)
        void SetRotationSensitivity(float s)
        {
            rotation_sensitivity_deg_per_pixel = s;
        }

        // CN: 获取旋转灵敏度（度/像素）
        // EN: Get rotation sensitivity (degrees per pixel)
        float GetRotationSensitivity()const{return rotation_sensitivity_deg_per_pixel;}

        // CN: 将当前设置填充到 Settings 结构，供 UI/配置系统使用
        // EN: Fill provided Settings struct with current values for UI/config use
        void FillSettings(Settings &s)const
        {
            s.rotation_sensitivity_deg_per_pixel = rotation_sensitivity_deg_per_pixel;
            s.invert_x = (input_invert_sign.x < 0.0f);
            s.invert_y = (input_invert_sign.y < 0.0f);
        }

        // CN: 从 Settings 结构加载设置，供 UI/配置系统使用
        // EN: Load settings from Settings struct (for UI/config systems)
        void ApplySettings(const Settings &s)
        {
            rotation_sensitivity_deg_per_pixel = s.rotation_sensitivity_deg_per_pixel;
            SetInvertAxis(s.invert_x, s.invert_y);
        }

        void SetTarget(const Vector3f &t) override
        {
            if(!camera) return;

            // Compute direction to target and initial forward
            Vector3f dir = t - camera->pos;
            float len = length(dir);

            if(len <= 1e-6f)
            {
                // target coincides with camera position
                forward = Vector3f(1,0,0);
                distance_to_target = 0.0f;
            }
            else
            {
                forward = dir / len; // normalized direction
                 // compute Euler from forward
                 pitch   = asin(forward.z);
                 yaw     = atan2(forward.y, forward.x);

                 // ensure forward vector matches yaw/pitch
                 UpdateCameraVector();

                 // scalar distance along forward
                 distance_to_target = dot(t - camera->pos, forward);
             }

            right   =normalize(cross(forward,camera->world_up));
            up      =normalize(cross(right,forward));

            camera->view_line=normalize(camera->pos-t);
        }

        void Refresh() override
        {
            if(!camera || !camera_info) return;

            target=camera->pos+forward*distance_to_target;

            camera_info->view       =LookAtMatrix(camera->pos,target,camera->world_up);

            RefreshCameraInfo(camera_info,vi,camera);

            if(ubo_camera_info) ubo_camera_info->Update();
        }

    public: // movement
            
        void UpdateCameraVector()
        {
            if(!camera) return;

            // Compute forward vector directly from yaw (azimuth) and pitch (elevation).
            // This avoids depending on external PolarToVector and gives stable behavior near the poles.
            const float cp = cosf(pitch);
            forward.x = cp * cosf(yaw);
            forward.y = cp * sinf(yaw);
            forward.z = sinf(pitch);

            right   =normalize(cross(forward,camera->world_up));
            up      =normalize(cross(right,forward));
        }

        void Forward(float move_step)
        {
            if(!camera) return;
            camera->pos+=forward*move_step;
        }

        void Backward(float move_step)
        {
            if(!camera) return;
            camera->pos-=forward*move_step;
        }

        void Left(float move_step)
        {
            if(!camera) return;
            camera->pos-=right*move_step;
        }

        void Right(float move_step)
        {
            if(!camera) return;
            camera->pos+=right*move_step;
        }

    public: // rotation

        // CN: 根据输入增量旋转（axis.x / axis.y 单位为像素）。应用灵敏度（度/像素）。
        // EN: Rotate by input delta (axis.x, axis.y are in pixels). Applies sensitivity (degrees/pixel).
        void Rotate(const Vector2f &axis)
        {
            constexpr float top_limit      =deg2rad( 89.0f);
            constexpr float bottom_limit   =deg2rad(-89.0f);

            // axis is mouse delta in pixels (after scaling in caller).
            // rotation_sensitivity_deg_per_pixel is degrees/pixel, convert to radians when applying.
            const float sens_rad = deg2rad(rotation_sensitivity_deg_per_pixel);

            yaw     -= axis.x * input_invert_sign.x * sens_rad;
            pitch   -= axis.y * input_invert_sign.y * sens_rad;

            // Normalize yaw to [-pi,pi] robustly
            const float PI = 3.14159265358979323846f;
            // Use fmod to keep yaw bounded, then shift to [-PI,PI]
            yaw = fmodf(yaw, 2.0f * PI);
            if(yaw > PI) yaw -= 2.0f * PI;
            else if(yaw < -PI) yaw += 2.0f * PI;

            // Clamp pitch to avoid flipping at the poles
            if(pitch>top_limit) pitch=top_limit;
            if(pitch<bottom_limit) pitch=bottom_limit;
            
            UpdateCameraVector();
        }

        void Move(const Vector3f &delta)
        {
            if(!camera) return;
            camera->pos+=delta;
        }
    };//class FirstPersonCameraControl:public CameraControl

    class CameraKeyboardControl:public io::KeyboardStateEvent
    {
        FirstPersonCameraControl *camera;
        float move_speed;

    public:

        CameraKeyboardControl(FirstPersonCameraControl *wc)
        {
            camera=wc;
            move_speed=1.0f;
        }

        io::EventProcResult OnPressed(const io::KeyboardButton &kb)override
        {
            if(io::KeyboardStateEvent::OnPressed(kb)==io::EventProcResult::Continue)
                return(io::EventProcResult::Continue);

            if(kb==io::KeyboardButton::Minus    )move_speed*=0.9f;else
            if(kb==io::KeyboardButton::Equals   )move_speed*=1.1f;

            return(io::EventProcResult::Break);
        }

        bool Update() override
        {
            // allow multiple keys to be processed simultaneously (remove else-if chain)
            if(HasPressed(io::KeyboardButton::W     )) { if(camera) camera->Forward   (move_speed); }
            if(HasPressed(io::KeyboardButton::S     )) { if(camera) camera->Backward  (move_speed); }
            if(HasPressed(io::KeyboardButton::A     )) { if(camera) camera->Left      (move_speed); }
            if(HasPressed(io::KeyboardButton::D     )) { if(camera) camera->Right     (move_speed); }

            return(true);
        }
    };//class CameraKeyboardControl:public io::KeyboardStateEvent

    class CameraMouseControl:public io::MouseEvent
    {
        FirstPersonCameraControl *camera;
        double cur_time;
        double last_time;

        Vector2f mouse_pos;
        Vector2f mouse_last_pos;

    protected:

        io::EventProcResult OnPressed(const Vector2i &mouse_coord,io::MouseButton) override
        {
            mouse_last_pos=mouse_coord;

            last_time=cur_time;

            return(io::EventProcResult::Break);
        }
    
        io::EventProcResult OnWheel(const Vector2i &mouse_coord) override
        {
            if(mouse_coord.y==0) return(io::EventProcResult::Continue);
            if(camera) camera->Forward(float(mouse_coord.y)/100.0f);

            return(io::EventProcResult::Break);
        }

        io::EventProcResult OnMove(const Vector2i &mouse_coord) override
        {
            mouse_pos=mouse_coord;

            bool left =HasPressed(io::MouseButton::Left);
            bool right=HasPressed(io::MouseButton::Right);

            Vector2f pos=mouse_coord;
            Vector2f gap=pos-mouse_last_pos;

            if(left)
            {
                gap/=-5.0f;

                if(camera) camera->Rotate(gap);
            }
            else
            if(right)
            {
                gap/=10.0f;

                if(camera) camera->Move(Vector3f(gap.x,0,gap.y));
            }

            last_time=cur_time;
            mouse_last_pos=pos;

            return(io::EventProcResult::Continue);
        }

    public:

        CameraMouseControl(FirstPersonCameraControl *wc)
        {
            camera=wc;
            cur_time=0;
            last_time=0;
        }

        const Vector2f &GetMouseCoord()const{return mouse_pos;}

        bool Update() override
        {
            cur_time=GetPreciseTime();

            return(true);
        }
    };//class CameraMouseControl:public io::MouseEvent
}//namespace hgl::graph
