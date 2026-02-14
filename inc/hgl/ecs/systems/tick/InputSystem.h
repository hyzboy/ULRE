#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/io/event/InputMapping.h>
#include<hgl/io/event/WindowEvent.h>
#include<hgl/io/event/MouseEvent.h>
#include<hgl/math/Vector.h>
#include <hgl/type/UnorderedMap.h>

namespace hgl::ecs
{
    using namespace hgl::math;

    class InputSystem : public ecs::System,io::WindowEvent
    {
    protected:

        Vector2i mouse_coord;
        bool mouse_buttons[3];          ///< 鼠标按钮状态 [0]=左键, [1]=右键, [2]=中键
        int wheel_delta;                ///< 滚轮增量（每帧重置）

        hgl::UnorderedMap<uint32_t, bool> key_states;  ///< 键盘状态映射

        io::InputMapper input_mapper;
        double current_time;

        void* mouse_capture_owner = nullptr;

        hgl::UnorderedMap<io::ActionID, bool> action_active;
        hgl::UnorderedMap<io::ActionID, bool> action_started;
        hgl::UnorderedMap<io::ActionID, bool> action_completed;
        hgl::UnorderedMap<io::ActionID, io::ActionValue> action_values;
        hgl::UnorderedMap<io::ActionID, float> action_analog_1d;

        virtual io::EventProcResult OnEvent(const io::EventHeader &header, const uint64 data) override;

        void OnActionEvent(const io::ActionEvent& evt);
        void ResetActionFrameState();

    public:

        io::EventDispatcher *GetEventDispatcher() { return this; }

        io::InputMapper &GetInputMapper() { return input_mapper; }

        const Vector2i &GetMouseCoord() const { return mouse_coord; }

        /// 查询鼠标按钮状态 / Query mouse button state
        /// @param button 鼠标按钮枚举值
        bool IsMouseButtonDown(io::MouseButton button) const;

        // 禁用隐式类型转换 / Disable implicit type conversions
        bool IsMouseButtonDown(int) const = delete;
        bool IsMouseButtonDown(unsigned int) const = delete;

        bool BeginMouseCapture(void* owner);
        void EndMouseCapture(void* owner);
        bool IsMouseCaptured() const { return mouse_capture_owner != nullptr; }
        bool IsMouseCapturedBy(const void* owner) const { return mouse_capture_owner == owner; }

        /// 获取滚轮增量 / Get wheel delta
        int GetWheelDelta() const { return wheel_delta; }

        /// 查询键盘按键状态 / Query key state
        /// @param key 键盘按键枚举值
        bool IsKeyDown(io::KeyboardButton key) const;

        // 禁用隐式类型转换 / Disable implicit type conversions
        bool IsKeyDown(int) const = delete;
        bool IsKeyDown(unsigned int) const = delete;
        bool IsKeyDown(char) const = delete;
        bool IsKeyDown(unsigned char) const = delete;

        /// 查询动作是否处于激活状态 / Check if action is active
        bool IsActionActive(io::ActionID action) const;

        /// 查询动作本帧是否开始 / Check if action started this frame
        bool WasActionStarted(io::ActionID action) const;

        /// 查询动作本帧是否结束 / Check if action completed this frame
        bool WasActionCompleted(io::ActionID action) const;

        /// 获取1D模拟量动作值（按帧累计） / Get analog 1D action value (per-frame)
        float GetActionAnalog1D(io::ActionID action) const;

        /// 获取动作最新值 / Get latest action value
        io::ActionValue GetActionValue(io::ActionID action) const;

    public:

        InputSystem();
        virtual ~InputSystem() = default;

        void Update(float deltaTime) override;
        void EndFrame();
    };
}//namespace hgl::ecs


