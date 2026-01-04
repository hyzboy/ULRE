#pragma once

#include<hgl/ecs/CameraController.h>
#include<hgl/io/event/MouseEvent.h>

namespace hgl::ecs
{
    using namespace hgl::math;
    
    /**
     * Model Viewer Camera
     * Specialized for viewing a centered model
     */
    class ModelViewerCamera : public CameraController
    {
    private:

        // Spherical coordinates (radians)
        float yaw = 0.0f;               // Azimuth
        float pitch = 0.0f;             // Elevation
        float distance = 5.0f;          // Distance from target to camera

        Vector3f target = Vector3f(0, 0, 0);  // Fixed model center

        // Local basis
        Vector3f forward = Vector3f(1, 0, 0);
        Vector3f right = Vector3f(0, 1, 0);
        Vector3f up = Vector3f(0, 0, 1);

        // Input configuration
        Vector2f input_invert_sign = Vector2f(-1, 1);      // x/y inversion for mouse
        float rotation_sensitivity_deg_per_pixel = 0.2f;   // Degrees per pixel
        float zoom_sensitivity = 0.1f;                      // Scale for wheel delta

        float min_distance = 0.1f;
        float max_distance = 1000.0f;

        bool dirty_basis = true;

        static inline void RecalcBasis(float yaw, float pitch, Vector3f& forward, Vector3f& right, Vector3f& up, const graph::Camera* cam)
        {
            const float cp = cosf(pitch);
            forward.x = cp * cosf(yaw);
            forward.y = cp * sinf(yaw);
            forward.z = sinf(pitch);

            Vector3f world_up(0, 0, 1);
            if (cam) world_up = cam->world_up;

            right = Normalized(Cross(forward, world_up));
            up = Normalized(Cross(right, forward));
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

        ModelViewerCamera() : CameraController()
        {
        }

        virtual ~ModelViewerCamera() = default;

        void SetTarget(const Vector3f& t) override
        {
            target = t;
            UpdateCameraVector();
        }

        void SetDistance(float d)
        {
            distance = std::clamp(d, min_distance, max_distance);
            UpdateCameraVector();
        }

        void SetMinMaxDistance(float min_d, float max_d)
        {
            min_distance = min_d;
            max_distance = max_d;
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

        void FillSettings(Settings& s) const
        {
            s.rotation_sensitivity_deg_per_pixel = rotation_sensitivity_deg_per_pixel;
            s.zoom_sensitivity = zoom_sensitivity;
            s.invert_x = (input_invert_sign.x < 0.0f);
            s.invert_y = (input_invert_sign.y < 0.0f);
            s.min_distance = min_distance;
            s.max_distance = max_distance;
        }

        void ApplySettings(const Settings& s)
        {
            rotation_sensitivity_deg_per_pixel = s.rotation_sensitivity_deg_per_pixel;
            zoom_sensitivity = s.zoom_sensitivity;
            SetInvertAxis(s.invert_x, s.invert_y);
            SetMinMaxDistance(s.min_distance, s.max_distance);
        }

        void UpdateCameraVector()
        {
            dirty_basis = true;
        }

        void UpdateCameraMatrix() override;

        void Rotate(const Vector2f& delta_pixels);

        void Zoom(float wheel_delta);
    };//class ModelViewerCamera

    /**
     * Mouse input helper for ModelViewer camera
     */
    class ModelViewerMouseInput : public io::MouseEvent
    {
        ModelViewerCamera* camera = nullptr;
        Vector2f mouse_last = Vector2f(0, 0);
        bool dragging = false;

    public:

        ModelViewerMouseInput(ModelViewerCamera* vc)
            : camera(vc), dragging(false)
        {
        }

        virtual ~ModelViewerMouseInput() = default;

        io::EventProcResult OnPressed(const Vector2i& mouse_coord, io::MouseButton mb) override;
        io::EventProcResult OnMove(const Vector2i& mouse_coord) override;
        io::EventProcResult OnWheel(const Vector2i& mouse_coord) override;
        io::EventProcResult OnReleased(const Vector2i& mouse_coord, io::MouseButton) override;

        bool Update() override { return true; }
    };
}//namespace hgl::ecs
