#pragma once

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace io
    {
        /**
         * EventProcResult - 事件处理结果 / Event processing result
         */
        enum class EventProcResult : uint8
        {
            Continue = 0,       ///< 继续传递事件 / Continue event propagation
            Break               ///< 中断事件传递 / Break event propagation
        };

        /**
         * InputEventSource - 输入事件源类型 / Input event source type
         */
        enum class InputEventSource : uint8
        {
            Keyboard = 0,       ///< 键盘 / Keyboard
            Mouse,              ///< 鼠标 / Mouse
            Joystick,           ///< 游戏手柄 / Joystick
            Touch               ///< 触摸 / Touch
        };

        /**
         * EventHeader - 事件头结构 / Event header structure
         */
        #pragma pack(push, 1)
        struct EventHeader
        {
            InputEventSource type;  ///< 事件源类型 / Event source type
            uint8 id;               ///< 事件ID(如MouseAction等) / Event ID (such as MouseAction, etc.)
            uint16 reserved;        ///< 保留字段 / Reserved field
            
            EventHeader() : type(InputEventSource::Keyboard), id(0), reserved(0) {}
        };
        #pragma pack(pop)

        /**
         * EventDispatcher - 事件分发器接口 / Event dispatcher interface
         */
        class EventDispatcher
        {
        public:
            virtual ~EventDispatcher() = default;

            /**
             * 处理事件 / Process event
             * @param header 事件头 / Event header
             * @param data 事件数据(通常是指向具体事件数据结构的指针) / Event data (usually pointer to specific event data structure)
             * @return 事件处理结果 / Event processing result
             */
            virtual EventProcResult OnEvent(const EventHeader &header, const uint64 data) = 0;
        };

    }//namespace io
}//namespace hgl
