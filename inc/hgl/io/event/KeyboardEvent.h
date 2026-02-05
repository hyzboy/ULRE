#pragma once

#include<hgl/type/DataType.h>
#include<hgl/io/event/EventDispatcher.h>

namespace hgl
{
    namespace io
    {
        /**
         * KeyboardButton - 键盘按键结构 / Keyboard button structure
         */
        struct KeyboardButton
        {
            uint32 key;         ///< 按键码 / Key code

            KeyboardButton() : key(0) {}
            explicit KeyboardButton(uint32 k) : key(k) {}

            // 常用按键常量 / Common key constants
            static constexpr uint32 W = 'W';
            static constexpr uint32 A = 'A';
            static constexpr uint32 S = 'S';
            static constexpr uint32 D = 'D';
            static constexpr uint32 Q = 'Q';
            static constexpr uint32 E = 'E';
        };

        /**
         * KeyboardEventID - 键盘事件类型枚举 / Keyboard event type enum
         */
        enum class KeyboardEventID : uint8
        {
            Down = 0,           ///< 按键按下 / Key down
            Up,                 ///< 按键抬起 / Key up
            Repeat              ///< 按键重复 / Key repeat
        };

        /**
         * KeyboardEventData - 键盘事件数据结构 / Keyboard event data structure
         */
        #pragma pack(push, 1)
        struct KeyboardEventData
        {
            uint32 key;         ///< 按键码 / Key code
            
            KeyboardEventData() : key(0) {}
            explicit KeyboardEventData(uint32 k) : key(k) {}
        };
        #pragma pack(pop)

        /**
         * KeyboardStateEvent - 键盘状态事件基类 / Keyboard state event base class
         */
        class KeyboardStateEvent : public EventDispatcher
        {
        protected:
            bool pressed[256];  ///< 按键按下状态 / Key pressed state

        public:
            KeyboardStateEvent()
            {
                for (int i = 0; i < 256; i++)
                    pressed[i] = false;
            }

            virtual ~KeyboardStateEvent() = default;

            /**
             * 检查按键是否按下 / Check if key is pressed
             */
            bool HasPressed(const KeyboardButton &kb) const
            {
                uint32 key = kb.key;
                if (key < 256)
                    return pressed[key];
                return false;
            }
            
            /**
             * 按键按下事件 / Key pressed event
             */
            virtual EventProcResult OnPressed(const KeyboardButton &kb) { return EventProcResult::Continue; }

            /**
             * 更新函数 / Update function
             */
            virtual bool Update() { return true; }

            /**
             * 处理事件 / Process event
             */
            EventProcResult OnEvent(const EventHeader &header, const uint64 data) override
            {
                if (header.type != InputEventSource::Keyboard)
                    return EventProcResult::Continue;

                const KeyboardEventData *ked = reinterpret_cast<const KeyboardEventData *>(&data);
                KeyboardEventID event_id = static_cast<KeyboardEventID>(header.id);

                switch (event_id)
                {
                    case KeyboardEventID::Down:
                        if (ked->key < 256)
                            pressed[ked->key] = true;
                        return OnPressed(KeyboardButton(ked->key));

                    case KeyboardEventID::Up:
                        if (ked->key < 256)
                            pressed[ked->key] = false;
                        return EventProcResult::Continue;

                    default:
                        return EventProcResult::Continue;
                }
            }
        };

    }//namespace io
}//namespace hgl
