#pragma once

#include<hgl/ecs/CameraController.h>

namespace hgl::ecs
{
    using namespace hgl::math;
    
    /**
     * Orbit Camera
     * Provides orbit/look-at style camera control
     */
    class OrbitCamera : public CameraController
    {
    protected:

        Vector3f direction = Vector3f(1, 0, 0);
        Vector3f right = Vector3f(0, 1, 0);
        Vector3f up = Vector3f(0, 0, 1);
        Vector3f target = Vector3f(0, 0, 0);

    public:

        OrbitCamera() : CameraController()
        {
        }

        virtual ~OrbitCamera() = default;

        void UpdateCameraMatrix() override;

        void SetTarget(const Vector3f& t) override
        {
            target = t;
        }

        void Move(const Vector3f& move_dist)
        {
            if (!camera) return;
            camera->pos += move_dist;
            target += move_dist;
        }

        void Rotate(double ang, const Vector3f& axis);

        void OrbitRotate(double ang, const Vector3f& axis);

        float GetDistance() const { return camera ? Length(camera->pos - target) : 0.0f; }

        void Distance(float rate)
        {
            if (!camera) return;
            if (rate == 1.0f) return;

            camera->pos = target + (camera->pos - target) * rate;
        }
    };//class OrbitCamera
}//namespace hgl::ecs
