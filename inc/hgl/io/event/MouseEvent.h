#pragma once

#include<hgl/type/DataType.h>
#include<hgl/io/event/EventDispatcher.h>
#include<hgl/math/Vector.h>

namespace hgl
{
    namespace io
    {
        /**
         * MouseButton - 鼠标按键枚举 / Mouse button enum
         */
        enum class MouseButton : uint8
        {
            Left = 0,
            Right = 1,
            Middle = 2
        };

        /**
         * MouseAction - 鼠标动作类型枚举 / Mouse action type enum
         * 表示鼠标的动作类型，与按键分离设计
         * Represents mouse action type, separated from button design
         */
        enum class MouseAction : uint8
        {
            Move = 0,           ///< 鼠标移动 / Mouse move
            Down,               ///< 按键按下 / Button down
            Up,                 ///< 按键抬起 / Button up
            Wheel               ///< 滚轮滚动 / Mouse wheel
        };

        /**
         * MouseEventData - 鼠标事件数据结构 / Mouse event data structure
         * 按键和动作分开设计：button字段记录哪个按键，action字段记录什么动作
         * Button and action are separated: button field records which button, action field records what action
         */
        #pragma pack(push, 1)
        struct MouseEventData
        {
            MouseAction action;     ///< 鼠标动作类型 / Mouse action type
            MouseButton button;     ///< 鼠标按键 / Mouse button
            int16 x;                ///< X坐标 / X coordinate
            int16 y;                ///< Y坐标 / Y coordinate
            int16 wheel_delta;      ///< 滚轮增量 / Wheel delta

            MouseEventData()
                : action(MouseAction::Move)
                , button(MouseButton::Left)
                , x(0)
                , y(0)
                , wheel_delta(0)
            {
            }
        };
        #pragma pack(pop)

        // 为了保持向后兼容，提供类型别名
        // For backward compatibility, provide type alias
        using MouseEventID = MouseAction;

        /**
         * MouseEvent - 鼠标事件基类 / Mouse event base class
         */
        class MouseEvent : public EventDispatcher
        {
        protected:
            bool pressed[3];    ///< 鼠标按键按下状态 / Mouse button pressed state

        public:
            MouseEvent()
            {
                pressed[0] = pressed[1] = pressed[2] = false;
            }

            virtual ~MouseEvent() = default;

            /**
             * 检查鼠标按键是否按下 / Check if mouse button is pressed
             */
            bool HasPressed(MouseButton mb) const
            {
                int idx = static_cast<int>(mb);
                if (idx < 0 || idx >= 3) return false;
                return pressed[idx];
            }

            /**
             * 鼠标按下事件 / Mouse button pressed event
             */
            virtual EventProcResult OnPressed(const math::Vector2i &mouse_coord, MouseButton mb) { return EventProcResult::Continue; }

            /**
             * 鼠标释放事件 / Mouse button released event
             */
            virtual EventProcResult OnReleased(const math::Vector2i &mouse_coord, MouseButton mb) { return EventProcResult::Continue; }

            /**
             * 鼠标移动事件 / Mouse move event
             */
            virtual EventProcResult OnMove(const math::Vector2i &mouse_coord) { return EventProcResult::Continue; }

            /**
             * 鼠标滚轮事件 / Mouse wheel event
             */
            virtual EventProcResult OnWheel(const math::Vector2i &wheel_delta) { return EventProcResult::Continue; }

            /**
             * 更新函数 / Update function
             */
            virtual bool Update() { return true; }

            /**
             * 处理事件 / Process event
             */
            EventProcResult OnEvent(const EventHeader &header, const uint64 data) override
            {
                if (header.type != InputEventSource::Mouse)
                    return EventProcResult::Continue;

                const MouseEventData *med = reinterpret_cast<const MouseEventData *>(&data);
                MouseAction action = static_cast<MouseAction>(header.id);
                MouseButton button = med->button;
                math::Vector2i coord(med->x, med->y);

                switch (action)
                {
                    case MouseAction::Down:
                    {
                        int idx = static_cast<int>(button);
                        if (idx >= 0 && idx < 3)
                            pressed[idx] = true;
                        return OnPressed(coord, button);
                    }

                    case MouseAction::Up:
                    {
                        int idx = static_cast<int>(button);
                        if (idx >= 0 && idx < 3)
                            pressed[idx] = false;
                        return OnReleased(coord, button);
                    }

                    case MouseAction::Move:
                        return OnMove(coord);

                    case MouseAction::Wheel:
                        return OnWheel(math::Vector2i(0, med->wheel_delta));

                    default:
                        return EventProcResult::Continue;
                }
            }
        };

    }//namespace io
}//namespace hgl
