#pragma once

#include<hgl/io/event/InputContext.h>
#include<hgl/io/event/InputMapping.h>
#include<hgl/io/event/InputTypes.h>

namespace hgl::ecs
{
    class CameraInputMapping
    {
    public:
#define HGL_CAMERA_ACTION_LIST(X) \
    X(MoveForward)            \
    X(MoveBackward)           \
    X(MoveLeft)               \
    X(MoveRight)              \
    X(MoveDown)               \
    X(MoveUp)                 \
    X(Rotate)                 \
    X(PanRight)               \
    X(PanMiddle)              \
    X(ZoomWheel)              \
    X(ZoomIn)                 \
    X(ZoomOut)                \
    X(ResetView)              \
    X(FocusView)

#define HGL_CAMERA_ACTION_ENUM(name) name,
    enum class Action : uint8
    {
        HGL_CAMERA_ACTION_LIST(HGL_CAMERA_ACTION_ENUM)
    };
#undef HGL_CAMERA_ACTION_ENUM

#define HGL_CAMERA_ACTION_CONST(name) \
    static constexpr io::ActionID kAction##name = io::MakeActionID(io::ActionDomain::Camera, Action::name);
    HGL_CAMERA_ACTION_LIST(HGL_CAMERA_ACTION_CONST)
#undef HGL_CAMERA_ACTION_CONST
#undef HGL_CAMERA_ACTION_LIST

        CameraInputMapping();

        void ApplyDefaultBindings();
        void EnsureContext(io::InputMapper& mapper);

        io::InputContext& EditContext();
        const io::InputContext& GetContext() const { return context; }

    private:
        io::InputContext context;
        bool configured;
        bool context_ready;
    };
}
