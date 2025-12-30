#pragma once

#include<hgl/graph/camera/CameraControl.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/time/Time.h>
#include <cmath>

namespace hgl::graph
{
    /**
     * ViewModelCameraControl
     * A camera controller specialized for viewing a centered model.
     * - The model (target) stays fixed.
     * - Left mouse drag: orbit rotation around the target (yaw, pitch).
     * - Mouse wheel: zoom in/out (change distance to target).
     */
    class ViewModelCameraControl: public CameraControl
    {
        // spherical coordinates (radians)
        float yaw;           // azimuth
        float pitch;         // elevation
        float distance;      // distance from target to camera

        Vector3f target;     // fixed model center

        // local basis
        Vector3f forward;
        Vector3f right;
        Vector3f up;

        // input configuration
        Vector2f input_invert_sign;                 // x/y inversion for mouse
        float rotation_sensitivity_deg_per_pixel;   // degrees per pixel
        float zoom_sensitivity;                     // scale applied to wheel delta

        float min_distance;
        float max_distance;

        bool  dirty_basis = true;

        static inline void RecalcBasis(float yaw,float pitch,math::Vector3f &forward,math::Vector3f &right,math::Vector3f &up,const Camera *cam)
        {
            const float cp = cosf(pitch);
            forward.x = cp * cosf(yaw);
            forward.y = cp * sinf(yaw);
            forward.z = sinf(pitch);

            Vector3f world_up(0,0,1);
            if(cam) world_up = cam->world_up;

            right = Normalize(Cross(forward,world_up));
            up    = Normalize(Cross(right,forward));
        }

    public:
        struct Settings
        {
            float rotation_sensitivity_deg_per_pixel;
            float zoom_sensitivity;
            bool invert_x;
            bool invert_y;
            float min_distance;
            float max_distance;
        };

        // Updated to follow CameraControl lifecycle: construct empty and rely on SetViewport/SetCamera
        ViewModelCameraControl()
        {
            yaw = 0.0f;
            pitch = 0.0f;
            distance = 5.0f;
            target = Vector3f(0,0,0);

            input_invert_sign = Vector2f(-1, 1);            // drag to orbit typical feel
            rotation_sensitivity_deg_per_pixel = 0.2f;
            zoom_sensitivity = 0.1f;

            min_distance = 0.1f;
            max_distance = 1000.0f;

            forward = Vector3f(1,0,0);
            right   = Vector3f(0,1,0);
            up      = Vector3f(0,0,1);

            dirty_basis = true;
        }

        virtual ~ViewModelCameraControl() = default;

        const CameraControlIDName GetControlName() const override
        {
            static CameraControlIDName VMCC("ViewModelCameraControl");
            return VMCC;
        }

        // Target is fixed: set initial target and distance
        void SetTarget(const Vector3f &t) override
        {
            target = t;
            UpdateCameraVector();
        }

        // Set initial spherical params
        void SetDistance(float d)
        {
            distance = std::clamp(d, min_distance, max_distance);
            UpdateCameraVector();
        }

        void SetMinMaxDistance(float min_d, float max_d)
        {
            min_distance = min_d; max_distance = max_d;
            distance = std::clamp(distance, min_distance, max_distance);
        }

        void SetRotationSensitivity(float deg_per_pixel)
        {
            rotation_sensitivity_deg_per_pixel = deg_per_pixel;
        }

        void SetZoomSensitivity(float s)
        {
            zoom_sensitivity = s;
        }

        void SetInvertAxis(bool ix, bool iy)
        {
            input_invert_sign.x = ix ? -1.0f : 1.0f;
            input_invert_sign.y = iy ? -1.0f : 1.0f;
        }

        void FillSettings(Settings &s) const
        {
            s.rotation_sensitivity_deg_per_pixel = rotation_sensitivity_deg_per_pixel;
            s.zoom_sensitivity = zoom_sensitivity;
            s.invert_x = (input_invert_sign.x < 0.0f);
            s.invert_y = (input_invert_sign.y < 0.0f);
            s.min_distance = min_distance;
            s.max_distance = max_distance;
        }

        void ApplySettings(const Settings &s)
        {
            rotation_sensitivity_deg_per_pixel = s.rotation_sensitivity_deg_per_pixel;
            zoom_sensitivity = s.zoom_sensitivity;
            SetInvertAxis(s.invert_x, s.invert_y);
            SetMinMaxDistance(s.min_distance, s.max_distance);
        }

        // mark basis dirty; will be recomputed in Refresh()
        void UpdateCameraVector()
        {
            dirty_basis = true;
        }

        // refresh camera_info (view matrix) — aligns with CameraControl::Refresh()
        void Refresh() override
        {
            Camera *cam = GetCamera();
            CameraInfo *ci = GetCameraInfo();
            const ViewportInfo *vi = GetViewportInfo();
            if(!cam || !ci || !vi)
                return;

            if(dirty_basis)
            {
                RecalcBasis(yaw,pitch,forward,right,up,cam);
                cam->pos = target - forward * distance;
                dirty_basis = false;
            }

            ci->view = LookAtMatrix(cam->pos, target, cam->world_up);
            RefreshCameraInfo(ci, vi, cam);
        }

    public:
        // allow programmatic rotation
        void Rotate(const Vector2f &delta_pixels)
        {
            const float sens_rad = deg2rad(rotation_sensitivity_deg_per_pixel);
            yaw   -= delta_pixels.x * input_invert_sign.x * sens_rad;
            pitch -= delta_pixels.y * input_invert_sign.y * sens_rad;

            // clamp pitch to avoid singularities
            const float top = deg2rad(89.0f);
            const float bottom = deg2rad(-89.0f);
            if(pitch > top) pitch = top;
            if(pitch < bottom) pitch = bottom;

            // normalize yaw
            const float PI = 3.14159265358979323846f;
            yaw = fmodf(yaw, 2.0f * PI);
            if(yaw > PI) yaw -= 2.0f * PI; else if(yaw < -PI) yaw += 2.0f * PI;

            UpdateCameraVector();
        }

        void Zoom(float wheel_delta)
        {
            // wheel_delta is integer steps (positive -> away). Treat positive as zoom out.
            distance *= std::pow(1.0f + zoom_sensitivity, -wheel_delta);
            distance = std::clamp(distance, min_distance, max_distance);
            UpdateCameraVector();
        }

    };

    // Mouse control helper for ViewModelCameraControl
    class ViewModelMouseControl: public io::MouseEvent
    {
        ViewModelCameraControl *camera;
        Vector2f mouse_last;
        bool dragging;

    public:
        ViewModelMouseControl(ViewModelCameraControl *vc):camera(vc),dragging(false){}

        io::EventProcResult OnPressed(const Vector2i &mouse_coord, io::MouseButton mb) override
        {
            mouse_last = Vector2f(mouse_coord);
            dragging = true;
            return io::EventProcResult::Break;
        }

        io::EventProcResult OnMove(const Vector2i &mouse_coord) override
        {
            if(!dragging) return io::EventProcResult::Continue;

            Vector2f pos(mouse_coord);
            Vector2f delta = pos - mouse_last;
            mouse_last = pos;

            if(camera)
            {
                // left button orbit
                if(HasPressed(io::MouseButton::Left)) camera->Rotate(delta);
            }

            return io::EventProcResult::Break;
        }

        io::EventProcResult OnWheel(const Vector2i &mouse_coord) override
        {
            // y is wheel delta
            if(camera && mouse_coord.y!=0) camera->Zoom(float(mouse_coord.y));
            return io::EventProcResult::Break;
        }

        io::EventProcResult OnReleased(const Vector2i &mouse_coord, io::MouseButton) override
        {
            dragging = false;
            return io::EventProcResult::Break;
        }

        bool Update() override { return true; }
    };

} // namespace hgl::graph
