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
         * MouseAction - 鼠标动作类型枚举(原MouseEventID) / Mouse action type enum (formerly MouseEventID)
         * 注意：MouseAction已经包含了按键信息(LeftDown/RightDown/MiddleDown等)，无需额外的button字段
         * Note: MouseAction already contains button info (LeftDown/RightDown/MiddleDown, etc.), no separate button field needed
         */
        enum class MouseAction : uint8
        {
            Move = 0,           ///< 鼠标移动 / Mouse move
            LeftDown,           ///< 左键按下 / Left button down
            LeftUp,             ///< 左键抬起 / Left button up
            RightDown,          ///< 右键按下 / Right button down
            RightUp,            ///< 右键抬起 / Right button up
            MiddleDown,         ///< 中键按下 / Middle button down
            MiddleUp,           ///< 中键抬起 / Middle button up
            Wheel               ///< 滚轮滚动 / Mouse wheel
        };

        /**
         * 从MouseAction提取MouseButton / Extract MouseButton from MouseAction
         * @param action 鼠标动作 / Mouse action
         * @return 对应的鼠标按键，如果无法确定则返回Left / Corresponding mouse button, returns Left if indeterminate
         */
        inline MouseButton GetButtonFromAction(MouseAction action)
        {
            switch(action)
            {
                case MouseAction::LeftDown:
                case MouseAction::LeftUp:
                    return MouseButton::Left;
                
                case MouseAction::RightDown:
                case MouseAction::RightUp:
                    return MouseButton::Right;
                
                case MouseAction::MiddleDown:
                case MouseAction::MiddleUp:
                    return MouseButton::Middle;
                
                default:
                    return MouseButton::Left; // Move和Wheel返回默认值
            }
        }

        /**
         * MouseEventData - 鼠标事件数据结构 / Mouse event data structure
         * action字段已包含按键信息，无需单独的button字段
         * The action field already contains button info, no separate button field needed
         */
        #pragma pack(push, 1)
        struct MouseEventData
        {
            MouseAction action;     ///< 鼠标动作类型(已包含按键信息) / Mouse action type (already contains button info)
            int16 x;                ///< X坐标 / X coordinate
            int16 y;                ///< Y坐标 / Y coordinate
            int16 wheel_delta;      ///< 滚轮增量 / Wheel delta

            MouseEventData()
                : action(MouseAction::Move)
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
                math::Vector2i coord(med->x, med->y);

                switch (action)
                {
                    case MouseAction::LeftDown:
                        pressed[0] = true;
                        return OnPressed(coord, MouseButton::Left);

                    case MouseAction::LeftUp:
                        pressed[0] = false;
                        return OnReleased(coord, MouseButton::Left);

                    case MouseAction::RightDown:
                        pressed[1] = true;
                        return OnPressed(coord, MouseButton::Right);

                    case MouseAction::RightUp:
                        pressed[1] = false;
                        return OnReleased(coord, MouseButton::Right);

                    case MouseAction::MiddleDown:
                        pressed[2] = true;
                        return OnPressed(coord, MouseButton::Middle);

                    case MouseAction::MiddleUp:
                        pressed[2] = false;
                        return OnReleased(coord, MouseButton::Middle);

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
