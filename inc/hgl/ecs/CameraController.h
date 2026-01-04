#pragma once

#include<hgl/ecs/InputSystem.h>
#include<hgl/graph/camera/Camera.h>

namespace hgl::ecs
{
    using namespace hgl::math;

    /**
     * Base Camera Controller System
     * Provides camera control functionality integrated with ECS InputSystem
     */
    class CameraController : public InputSystem
    {
    protected:

        graph::Camera* camera = nullptr;
        graph::CameraInfo* camera_info = nullptr;
        const graph::ViewportInfo* viewport_info = nullptr;

    public:

        CameraController() : InputSystem() {}
        virtual ~CameraController() = default;

        void SetCamera(graph::Camera* cam, graph::CameraInfo* info, const graph::ViewportInfo* vi)
        {
            camera = cam;
            camera_info = info;
            viewport_info = vi;
        }

        graph::Camera* GetCamera() const { return camera; }
        graph::CameraInfo* GetCameraInfo() const { return camera_info; }
        const graph::ViewportInfo* GetViewportInfo() const { return viewport_info; }

        void ZoomFOV(int adjust)
        {
            if (!camera)
                return;

            constexpr float MinFOV = 10;
            constexpr float MaxFOV = 180;

            camera->fovY += float(adjust) / 10.0f;

            if (adjust < 0 && camera->fovY < MinFOV)
                camera->fovY = MinFOV;
            else if (adjust > 0 && camera->fovY > MaxFOV)
                camera->fovY = MaxFOV;
        }

        virtual void SetPosition(const Vector3f& pos)
        {
            if (camera)
                camera->pos = pos;
        }

        virtual void SetTarget(const Vector3f& target) = 0;
        virtual void UpdateCameraMatrix() = 0;
    };
}//namespace hgl::ecs
