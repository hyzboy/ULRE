#pragma once

#include<hgl/io/event/EventDispatcher.h>

namespace hgl
{
    namespace io
    {
        /**
         * WindowEvent - 窗口事件基类 / Window event base class
         * 提供基础的事件分发机制 / Provides basic event dispatching mechanism
         */
        class WindowEvent : public EventDispatcher
        {
        public:
            WindowEvent() = default;
            virtual ~WindowEvent() = default;

            /**
             * 处理事件 / Process event
             * 默认实现直接返回Continue，子类可以重写以处理特定事件
             * Default implementation returns Continue, subclasses can override to handle specific events
             */
            EventProcResult OnEvent(const EventHeader &header, const uint64 data) override
            {
                return EventProcResult::Continue;
            }
        };

    }//namespace io
}//namespace hgl
