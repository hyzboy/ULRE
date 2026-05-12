#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl
{
    namespace graph
    {
        struct CameraInfo;
        class ViewportInfo;
    }
}

namespace hgl::ecs
{
    class TransformComponent;
    class BillboardScaleComponent;

    class BillboardScaleSystem : public System
    {
    private:

        class ECSContext* world = nullptr;
        const graph::CameraInfo* camera_info = nullptr;
        const graph::ViewportInfo* viewport_info = nullptr;

    public:

        BillboardScaleSystem(const std::string& name = "BillboardScaleSystem");
        ~BillboardScaleSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetCameraInfo(const graph::CameraInfo* info) { camera_info = info; }
        void SetViewportInfo(const graph::ViewportInfo* info) { viewport_info = info; }

        const graph::CameraInfo* GetCameraInfo() const { return camera_info; }
        const graph::ViewportInfo* GetViewportInfo() const { return viewport_info; }

    public:

        void Update(float deltaTime) override;

    private:

        bool UpdateBillboardScale(BillboardScaleComponent* scale,
                                  TransformComponent* transform);
    };
}//namespace hgl::ecs
