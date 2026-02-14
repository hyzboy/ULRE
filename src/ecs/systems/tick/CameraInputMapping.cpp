#include<hgl/ecs/systems/tick/CameraInputMapping.h>

namespace hgl::ecs
{
    CameraInputMapping::CameraInputMapping()
        : configured(false)
        , context_ready(false)
    {
    }

    void CameraInputMapping::ApplyDefaultBindings()
    {
        context.Clear();
        context.BindKey(kActionMoveForward, io::KeyboardButton::W);
        context.BindKey(kActionMoveBackward, io::KeyboardButton::S);
        context.BindKey(kActionMoveLeft, io::KeyboardButton::A);
        context.BindKey(kActionMoveRight, io::KeyboardButton::D);
        context.BindKey(kActionMoveDown, io::KeyboardButton::Q);
        context.BindKey(kActionMoveUp, io::KeyboardButton::E);

        context.BindMouse(kActionRotate, io::MouseButton::Left);
        context.BindMouse(kActionPanRight, io::MouseButton::Right);
        context.BindMouse(kActionPanMiddle, io::MouseButton::Mid);

        context.BindMouseWheel(kActionZoomWheel);
        context.BindKey(kActionZoomIn, io::KeyboardButton::PageUp);
        context.BindKey(kActionZoomIn, io::KeyboardButton::Equals);
        context.BindKey(kActionZoomOut, io::KeyboardButton::PageDown);
        context.BindKey(kActionZoomOut, io::KeyboardButton::Minus);

        context.BindKey(kActionResetView, io::KeyboardButton::R);
        context.BindKey(kActionFocusView, io::KeyboardButton::F);

        configured = true;
    }

    void CameraInputMapping::EnsureContext(io::InputMapper& mapper)
    {
        if (context_ready)
            return;

        if (!configured)
            ApplyDefaultBindings();

        mapper.PushContext(&context);
        context_ready = true;
    }

    io::InputContext& CameraInputMapping::EditContext()
    {
        configured = true;
        return context;
    }
}

