#include<hgl/ecs/InputSystem.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/io/event/KeyboardEvent.h>
#include<iostream>

namespace hgl::ecs
{
    InputSystem::InputSystem()
    {
        mouse_coord.Set(0, 0);
        mouse_buttons[0] = false;
        mouse_buttons[1] = false;
        mouse_buttons[2] = false;
        wheel_delta = 0;
    }

    io::EventProcResult InputSystem::OnEvent(const io::EventHeader &header, const uint64 data)
    {
        // 处理鼠标事件 / Handle mouse events
        if (header.type == io::InputEventSource::Mouse)
        {
            // 使用MouseAction替代MouseEventID / Use MouseAction instead of MouseEventID
            io::MouseAction action = io::MouseAction(header.id);
            const io::MouseEventData *med = (const io::MouseEventData *)&data;

            switch (action)
            {
                case io::MouseAction::Move:
                    mouse_coord.x = med->x;
                    mouse_coord.y = med->y;
                    break;

                case io::MouseAction::LeftDown:
                    mouse_buttons[0] = true;
                    break;

                case io::MouseAction::LeftUp:
                    mouse_buttons[0] = false;
                    break;

                case io::MouseAction::RightDown:
                    mouse_buttons[1] = true;
                    break;

                case io::MouseAction::RightUp:
                    mouse_buttons[1] = false;
                    break;

                case io::MouseAction::MiddleDown:
                    mouse_buttons[2] = true;
                    break;

                case io::MouseAction::MiddleUp:
                    mouse_buttons[2] = false;
                    break;

                case io::MouseAction::Wheel:
                    wheel_delta += med->wheel_delta;
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
                case io::KeyboardEventID::Down:
                    key_states[ked->key] = true;
                    break;

                case io::KeyboardEventID::Up:
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

    void InputSystem::Update(float deltaTime)
    {
        // 重置帧间状态 / Reset per-frame state
        wheel_delta = 0;
    }
}//namespace hgl::ecs
