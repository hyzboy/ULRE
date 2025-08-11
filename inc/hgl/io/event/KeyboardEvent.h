#pragma once

#include <hgl/io/event/WindowEvent.h>
#include <set>

namespace hgl::io
{
    /**
     * 键盘事件处理类
     */
    class KeyboardEvent : public WindowEvent
    {
        std::set<uint> pressed_keys;     ///< 当前按下的按键集合

    public:
        KeyboardEvent() = default;
        virtual ~KeyboardEvent() = default;

        /**
         * 检查按键是否被按下
         */
        bool IsKeyPressed(uint key) const
        {
            return pressed_keys.find(key) != pressed_keys.end();
        }

        /**
         * 获取当前按下的所有按键
         */
        const std::set<uint>& GetPressedKeys() const { return pressed_keys; }

        /**
         * 处理按键按下事件
         */
        void OnKeyDown(uint key) override
        {
            pressed_keys.insert(key);
        }

        /**
         * 处理按键释放事件
         */
        void OnKeyUp(uint key) override
        {
            pressed_keys.erase(key);
        }

        /**
         * 清空所有按键状态
         */
        void ClearAllKeys()
        {
            pressed_keys.clear();
        }

        /**
         * 更新键盘状态（在帧循环中调用）
         */
        virtual void Update()
        {
            // 在这里可以处理键盘状态的持续更新
        }

    protected:
        /**
         * 进入独占模式时的处理
         */
        void OnEnterExclusiveMode(EventDispatcher* exclusive_dispatcher) override
        {
            // 可以在这里添加键盘独占模式的特殊处理
        }

        /**
         * 退出独占模式时的处理
         */
        void OnExitExclusiveMode(EventDispatcher* old_exclusive_dispatcher) override
        {
            // 可以在这里添加退出键盘独占模式的处理
        }
    };

} // namespace hgl::io