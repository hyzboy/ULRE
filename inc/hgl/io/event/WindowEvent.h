#pragma once

#include <hgl/io/event/EventDispatcher.h>

namespace hgl::io
{
    /**
     * 窗口事件处理基类
     * 扩展EventDispatcher以支持窗口相关事件
     */
    class WindowEvent : public EventDispatcher
    {
    public:
        WindowEvent() = default;
        virtual ~WindowEvent() = default;

        // 窗口事件处理方法
        virtual void OnResize(uint w, uint h) {}
        virtual void OnActive(bool active) {}
        virtual void OnClose() {}
        virtual void OnKeyDown(uint key) {}
        virtual void OnKeyUp(uint key) {}
        virtual void OnMouseMove(int x, int y) {}
        virtual void OnMouseDown(uint button) {}
        virtual void OnMouseUp(uint button) {}
        virtual void OnMouseWheel(int delta) {}

    protected:
        /**
         * 处理事件分发
         */
        void HandleEvent(const void* event) override
        {
            // 这里可以根据事件类型进行具体的分发
            // 由于事件类型系统未完全定义，这里提供基础框架
        }

        /**
         * 分发窗口事件到子分发器
         */
        template<typename... Args>
        void DispatchWindowEvent(void (WindowEvent::*method)(Args...), Args... args)
        {
            // 处理当前对象的事件
            (this->*method)(args...);

            // 然后分发给子分发器
            if (is_exclusive_mode && exclusive_dispatcher)
            {
                // 独占模式下只分发给独占分发器
                WindowEvent* child_window_event = dynamic_cast<WindowEvent*>(exclusive_dispatcher);
                if (child_window_event)
                {
                    child_window_event->DispatchWindowEvent(method, args...);
                }
            }
            else
            {
                // 正常模式下分发给所有子分发器
                for (EventDispatcher* child : child_dispatchers)
                {
                    WindowEvent* child_window_event = dynamic_cast<WindowEvent*>(child);
                    if (child_window_event)
                    {
                        child_window_event->DispatchWindowEvent(method, args...);
                    }
                }
            }
        }

    public:
        /**
         * 触发窗口大小改变事件
         */
        void TriggerResize(uint w, uint h)
        {
            DispatchWindowEvent(&WindowEvent::OnResize, w, h);
        }

        /**
         * 触发窗口激活状态改变事件
         */
        void TriggerActive(bool active)
        {
            DispatchWindowEvent(&WindowEvent::OnActive, active);
        }

        /**
         * 触发窗口关闭事件
         */
        void TriggerClose()
        {
            DispatchWindowEvent(&WindowEvent::OnClose);
        }

        /**
         * 触发键盘按下事件
         */
        void TriggerKeyDown(uint key)
        {
            DispatchWindowEvent(&WindowEvent::OnKeyDown, key);
        }

        /**
         * 触发键盘释放事件
         */
        void TriggerKeyUp(uint key)
        {
            DispatchWindowEvent(&WindowEvent::OnKeyUp, key);
        }

        /**
         * 触发鼠标移动事件
         */
        void TriggerMouseMove(int x, int y)
        {
            DispatchWindowEvent(&WindowEvent::OnMouseMove, x, y);
        }

        /**
         * 触发鼠标按下事件
         */
        void TriggerMouseDown(uint button)
        {
            DispatchWindowEvent(&WindowEvent::OnMouseDown, button);
        }

        /**
         * 触发鼠标释放事件
         */
        void TriggerMouseUp(uint button)
        {
            DispatchWindowEvent(&WindowEvent::OnMouseUp, button);
        }

        /**
         * 触发鼠标滚轮事件
         */
        void TriggerMouseWheel(int delta)
        {
            DispatchWindowEvent(&WindowEvent::OnMouseWheel, delta);
        }
    };

} // namespace hgl::io