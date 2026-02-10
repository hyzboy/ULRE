#include<hgl/ecs/InputSystem.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<iostream>

namespace hgl::ecs
{
    InputSystem::InputSystem()
    {
        // Set system type and properties
        SetSystemType(SystemType::Input);
        SetExecutionOrder(0);  // Run first
        
        // No dependencies - Input runs first
        
        mouse_coord.x = 0;
        mouse_coord.y = 0;
        mouse_buttons[0] = false;
        mouse_buttons[1] = false;
        mouse_buttons[2] = false;
        wheel_delta = 0;
        current_time = 0.0;

        input_mapper.SetActionCallback([this](const io::ActionEvent& evt)
        {
            OnActionEvent(evt);
        });
    }

    io::EventProcResult InputSystem::OnEvent(const io::EventHeader &header, const uint64 data)
    {
        input_mapper.SetCurrentTime(current_time);
        input_mapper.ProcessPhysicalInput(header, data);

        // 处理鼠标事件 / Handle mouse events
        if (header.type == io::InputEventSource::Mouse)
        {
            // 使用分离的button和action设计 / Use separated button and action design
            io::MouseAction action = io::MouseAction(header.id);
            const io::MouseEventData *med = (const io::MouseEventData *)&data;
            io::MouseButton button = static_cast<io::MouseButton>(med->button);

            switch (action)
            {
                case io::MouseAction::Move:
                    mouse_coord.x = med->x;
                    mouse_coord.y = med->y;
                    break;

                case io::MouseAction::Pressed:
                {
                    int idx = static_cast<int>(button);
                    if (idx >= 0 && idx < 3)
                        mouse_buttons[idx] = true;
                    break;
                }

                case io::MouseAction::Released:
                {
                    int idx = static_cast<int>(button);
                    if (idx >= 0 && idx < 3)
                        mouse_buttons[idx] = false;
                    break;
                }

                case io::MouseAction::Wheel:
                    wheel_delta += med->y;
                    break;

                default:
                    break;
            }
        }
        // 处理键盘事件 / Handle keyboard events
        else if (header.type == io::InputEventSource::Keyboard)
        {
            io::KeyboardEventID event_id = io::KeyboardEventID(header.id);
            const io::KeyboardEventData *ked = (const io::KeyboardEventData *)&data;

            switch (event_id)
            {
                case io::KeyboardEventID::Pressed:
                    key_states[ked->key] = true;
                    break;

                case io::KeyboardEventID::Released:
                    key_states[ked->key] = false;
                    break;

                default:
                    break;
            }
        }

        return io::WindowEvent::OnEvent(header, data);
    }

    bool InputSystem::IsMouseButtonDown(int button) const
    {
        if (button < 0 || button >= 3)
            return false;
        return mouse_buttons[button];
    }

    bool InputSystem::IsKeyDown(uint32_t keycode) const
    {
        auto it = key_states.find(keycode);
        if (it != key_states.end())
            return it->second;
        return false;
    }

    bool InputSystem::IsActionActive(io::ActionID action) const
    {
        auto it = action_active.find(action);
        if (it != action_active.end())
            return it->second;
        return false;
    }

    bool InputSystem::WasActionStarted(io::ActionID action) const
    {
        auto it = action_started.find(action);
        if (it != action_started.end())
            return it->second;
        return false;
    }

    bool InputSystem::WasActionCompleted(io::ActionID action) const
    {
        auto it = action_completed.find(action);
        if (it != action_completed.end())
            return it->second;
        return false;
    }

    float InputSystem::GetActionAnalog1D(io::ActionID action) const
    {
        auto it = action_analog_1d.find(action);
        if (it != action_analog_1d.end())
            return it->second;
        return 0.0f;
    }

    io::ActionValue InputSystem::GetActionValue(io::ActionID action) const
    {
        auto it = action_values.find(action);
        if (it != action_values.end())
            return it->second;
        return io::ActionValue();
    }

    void InputSystem::Update(float deltaTime)
    {
        current_time += static_cast<double>(deltaTime);
        input_mapper.SetCurrentTime(current_time);

        // 重置帧间状态 / Reset per-frame state
        wheel_delta = 0;
        ResetActionFrameState();
    }

    void InputSystem::OnActionEvent(const io::ActionEvent& evt)
    {
        action_values[evt.action] = evt.value;

        switch (evt.state)
        {
            case io::ActionEventState::Started:
                action_active[evt.action] = true;
                action_started[evt.action] = true;
                break;

            case io::ActionEventState::Ongoing:
                if (evt.value.type == io::ActionValueType::Digital)
                    action_active[evt.action] = evt.value.digital;
                break;

            case io::ActionEventState::Completed:
                action_active[evt.action] = false;
                action_completed[evt.action] = true;
                break;

            case io::ActionEventState::Canceled:
                action_active[evt.action] = false;
                action_completed[evt.action] = true;
                break;
        }

        if (evt.value.type == io::ActionValueType::Analog1D)
            action_analog_1d[evt.action] += evt.value.analog_1d;
    }

    void InputSystem::ResetActionFrameState()
    {
        action_started.clear();
        action_completed.clear();
        action_analog_1d.clear();
    }
}//namespace hgl::ecs
