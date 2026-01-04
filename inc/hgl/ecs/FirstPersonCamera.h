#pragma once

#include<hgl/ecs/CameraController.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<hgl/time/Time.h>

namespace hgl::ecs
{
    using namespace hgl::math;

    /**
     * First-Person Camera
     * Provides FPS-style camera control integrated with ECS
     */
    class FirstPersonCamera : public CameraController
    {
    private:

        // Euler angles (in radians)
        float pitch = 0.0f;         // Rotation about X axis (elevation)
        float yaw = 0.0f;           // Rotation about Z axis (azimuth, Z is up)
        float roll = 0.0f;          // Rotation about Y axis (bank)

        // Local camera basis vectors
        Vector3f forward = Vector3f(1, 0, 0);
        Vector3f right = Vector3f(0, 1, 0);
        Vector3f up = Vector3f(0, 0, 1);

        // Input axis inversion flags (+1 or -1)
        Vector2f input_invert_sign = Vector2f(1, 1);

        // Rotation sensitivity (degrees per pixel)
        float rotation_sensitivity_deg_per_pixel = 0.2f;

        double last_time = 0.0;

    public:

        struct Settings
        {
            float rotation_sensitivity_deg_per_pixel;
            bool invert_x;
            bool invert_y;
        };

        FirstPersonCamera() : CameraController()
        {
        }

        virtual ~FirstPersonCamera() = default;

        void SetInvertAxis(bool invert_x, bool invert_y)
        {
            input_invert_sign.x = invert_x ? -1.0f : 1.0f;
            input_invert_sign.y = invert_y ? -1.0f : 1.0f;
        }

        void GetInvertAxis(bool& out_invert_x, bool& out_invert_y) const
        {
            out_invert_x = (input_invert_sign.x < 0.0f);
            out_invert_y = (input_invert_sign.y < 0.0f);
        }

        void SetRotationSensitivity(float s)
        {
            rotation_sensitivity_deg_per_pixel = s;
        }

        float GetRotationSensitivity() const { return rotation_sensitivity_deg_per_pixel; }

        void FillSettings(Settings& s) const
        {
            s.rotation_sensitivity_deg_per_pixel = rotation_sensitivity_deg_per_pixel;
            s.invert_x = (input_invert_sign.x < 0.0f);
            s.invert_y = (input_invert_sign.y < 0.0f);
        }

        void ApplySettings(const Settings& s)
        {
            rotation_sensitivity_deg_per_pixel = s.rotation_sensitivity_deg_per_pixel;
            SetInvertAxis(s.invert_x, s.invert_y);
        }

        void SetTarget(const Vector3f& t) override { /* FPS doesn't use target */ }

        void UpdateCameraMatrix() override;

        void UpdateCameraVector();

        void Forward(float move_step)
        {
            if (!camera) return;
            camera->pos += forward * move_step;
        }

        void Backward(float move_step)
        {
            if (!camera) return;
            camera->pos -= forward * move_step;
        }

        void Left(float move_step)
        {
            if (!camera) return;
            camera->pos -= right * move_step;
        }

        void Right(float move_step)
        {
            if (!camera) return;
            camera->pos += right * move_step;
        }

        void Rotate(const Vector2f& axis);

        void Move(const Vector3f& delta)
        {
            if (!camera) return;
            camera->pos += delta;
        }
    };//class FirstPersonCamera : public CameraController
    
    /**
     * Mouse input helper for FirstPerson camera
     */
    class FirstPersonMouseInput : public io::MouseEvent
    {
        FirstPersonCamera* camera = nullptr;
        double cur_time = 0;
        double last_time = 0;
        Vector2f mouse_pos = Vector2f(0, 0);
        Vector2f mouse_last_pos = Vector2f(0, 0);

    protected:

        io::EventProcResult OnPressed(const Vector2i& mouse_coord, io::MouseButton) override;
        io::EventProcResult OnWheel(const Vector2i& mouse_coord) override;
        io::EventProcResult OnMove(const Vector2i& mouse_coord) override;

    public:

        FirstPersonMouseInput(FirstPersonCamera* cam)
            : camera(cam)
        {
        }

        virtual ~FirstPersonMouseInput() = default;

        const Vector2f& GetMouseCoord() const { return mouse_pos; }

        bool Update() override
        {
            cur_time = GetPreciseTime();
            return true;
        }
    };//class FirstPersonMouseInput
    
    /**
     * Keyboard input helper for FirstPerson camera
     */
    class FirstPersonKeyboardInput : public io::KeyboardStateEvent
    {
        FirstPersonCamera* camera = nullptr;
        float move_speed = 1.0f;

    public:

        FirstPersonKeyboardInput(FirstPersonCamera* cam)
            : camera(cam)
        {
        }

        virtual ~FirstPersonKeyboardInput() = default;

        io::EventProcResult OnPressed(const io::KeyboardButton& kb) override;

        bool Update() override;
    };//class FirstPersonKeyboardInput
}//namespace hgl::ecs
