#pragma once

#include <hgl/io/event/WindowEvent.h>
#include <string>

// 基本类型定义（独立于submodule）
using OSString = std::string;

namespace hgl
{
    /**
     * 窗口类
     * 基础窗口抽象，支持事件分发
     */
    class Window : public io::WindowEvent
    {
        OSString window_title;      ///< 窗口标题
        uint window_width = 0;      ///< 窗口宽度
        uint window_height = 0;     ///< 窗口高度
        bool is_visible = false;    ///< 是否可见
        bool is_active = false;     ///< 是否激活

    public:
        Window() = default;
        Window(const OSString& title) : window_title(title) {}
        virtual ~Window() = default;

        /**
         * 创建窗口
         */
        virtual bool Create(uint w, uint h)
        {
            window_width = w;
            window_height = h;
            is_visible = true;
            return true;
        }

        /**
         * 更新窗口状态
         */
        virtual bool Update()
        {
            // 基础实现，子类应该重写
            return is_visible;
        }

        /**
         * 设置窗口标题
         */
        virtual void SetTitle(const OSString& title)
        {
            window_title = title;
        }

        /**
         * 获取窗口标题
         */
        const OSString& GetTitle() const { return window_title; }

        /**
         * 获取窗口尺寸
         */
        uint GetWidth() const { return window_width; }
        uint GetHeight() const { return window_height; }

        /**
         * 设置窗口尺寸
         */
        virtual void Resize(uint w, uint h)
        {
            if (window_width != w || window_height != h)
            {
                window_width = w;
                window_height = h;
                TriggerResize(w, h);
            }
        }

        /**
         * 是否可见
         */
        bool IsVisible() const { return is_visible; }

        /**
         * 设置可见性
         */
        virtual void SetVisible(bool visible)
        {
            is_visible = visible;
        }

        /**
         * 是否激活
         */
        bool IsActive() const { return is_active; }

        /**
         * 设置激活状态
         */
        virtual void SetActive(bool active)
        {
            if (is_active != active)
            {
                is_active = active;
                TriggerActive(active);
            }
        }

        /**
         * 关闭窗口
         */
        virtual void Close()
        {
            is_visible = false;
            TriggerClose();
        }

        /**
         * 模拟鼠标移动（用于测试）
         */
        void SimulateMouseMove(int x, int y)
        {
            TriggerMouseMove(x, y);
        }

        /**
         * 模拟鼠标按下（用于测试）
         */
        void SimulateMouseDown(uint button)
        {
            TriggerMouseDown(button);
        }

        /**
         * 模拟鼠标释放（用于测试）
         */
        void SimulateMouseUp(uint button)
        {
            TriggerMouseUp(button);
        }

        /**
         * 模拟按键按下（用于测试）
         */
        void SimulateKeyDown(uint key)
        {
            TriggerKeyDown(key);
        }

        /**
         * 模拟按键释放（用于测试）
         */
        void SimulateKeyUp(uint key)
        {
            TriggerKeyUp(key);
        }
    };

    /**
     * 创建渲染窗口（平台相关实现）
     */
    Window* CreateRenderWindow(const OSString& title);

    /**
     * 初始化本地窗口系统
     */
    void InitNativeWindowSystem();

} // namespace hgl