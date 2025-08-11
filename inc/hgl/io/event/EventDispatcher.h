#pragma once

#include <vector>
#include <algorithm>

// 基本类型定义（独立于submodule）
using uint = unsigned int;

namespace hgl::io
{
    /**
     * 事件分发器基类
     * 支持层级事件分发和独占模式
     */
    class EventDispatcher
    {
    protected:
        std::vector<EventDispatcher*> child_dispatchers;  ///< 子事件分发器列表
        EventDispatcher* parent_dispatcher = nullptr;     ///< 父事件分发器
        EventDispatcher* exclusive_dispatcher = nullptr;  ///< 独占模式的事件分发器
        bool is_exclusive_mode = false;                   ///< 是否处于独占模式

    public:
        EventDispatcher() = default;
        virtual ~EventDispatcher() = default;

        /**
         * 添加子事件分发器
         */
        virtual bool AddChildDispatcher(EventDispatcher* dispatcher)
        {
            if (!dispatcher) return false;
            
            // 检查是否已存在
            auto it = std::find(child_dispatchers.begin(), child_dispatchers.end(), dispatcher);
            if (it != child_dispatchers.end()) return false;
            
            child_dispatchers.push_back(dispatcher);
            dispatcher->parent_dispatcher = this;
            return true;
        }

        /**
         * 移除子事件分发器
         */
        virtual bool RemoveChildDispatcher(EventDispatcher* dispatcher)
        {
            if (!dispatcher) return false;
            
            auto it = std::find(child_dispatchers.begin(), child_dispatchers.end(), dispatcher);
            if (it != child_dispatchers.end())
            {
                child_dispatchers.erase(it);
                dispatcher->parent_dispatcher = nullptr;
                
                // 如果移除的是独占分发器，退出独占模式
                if (exclusive_dispatcher == dispatcher)
                {
                    ExitExclusiveMode();
                }
                return true;
            }
            return false;
        }

        /**
         * 设置独占模式事件分发器
         * @param dispatcher 要设置为独占的事件分发器，必须是当前分发器的子分发器
         * @return 是否成功设置
         */
        virtual bool SetExclusiveMode(EventDispatcher* dispatcher)
        {
            if (!dispatcher) return false;
            
            // 检查是否是子分发器
            auto it = std::find(child_dispatchers.begin(), child_dispatchers.end(), dispatcher);
            if (it == child_dispatchers.end()) return false;
            
            // 如果已经有其他独占分发器，先退出
            if (exclusive_dispatcher && exclusive_dispatcher != dispatcher)
            {
                ExitExclusiveMode();
            }
            
            exclusive_dispatcher = dispatcher;
            is_exclusive_mode = true;
            
            OnEnterExclusiveMode(dispatcher);
            return true;
        }

        /**
         * 退出独占模式
         */
        virtual void ExitExclusiveMode()
        {
            if (is_exclusive_mode && exclusive_dispatcher)
            {
                EventDispatcher* old_exclusive = exclusive_dispatcher;
                exclusive_dispatcher = nullptr;
                is_exclusive_mode = false;
                
                OnExitExclusiveMode(old_exclusive);
            }
        }

        /**
         * 获取当前独占模式的事件分发器
         */
        EventDispatcher* GetExclusiveDispatcher() const { return exclusive_dispatcher; }

        /**
         * 是否处于独占模式
         */
        bool IsExclusiveMode() const { return is_exclusive_mode; }

        /**
         * 获取父事件分发器
         */
        EventDispatcher* GetParentDispatcher() const { return parent_dispatcher; }

        /**
         * 获取子事件分发器列表
         */
        const std::vector<EventDispatcher*>& GetChildDispatchers() const { return child_dispatchers; }

        /**
         * 请求在更高级别处于独占状态
         * @param levels 向上提升的级别数，0表示在当前级别，1表示在父级别
         * @return 是否成功设置独占模式
         */
        virtual bool RequestExclusiveAtHigherLevel(int levels = 1)
        {
            if (levels <= 0)
            {
                // 在当前级别设置独占模式（如果有父分发器）
                if (parent_dispatcher)
                {
                    return parent_dispatcher->SetExclusiveMode(this);
                }
                return false;
            }
            
            // 递归向上查找指定级别的父分发器
            EventDispatcher* target_parent = parent_dispatcher;
            for (int i = 1; i < levels && target_parent; i++)
            {
                target_parent = target_parent->GetParentDispatcher();
            }
            
            if (target_parent && target_parent->GetParentDispatcher())
            {
                return target_parent->GetParentDispatcher()->SetExclusiveMode(target_parent);
            }
            
            return false;
        }

        /**
         * 释放在更高级别的独占状态
         * @param levels 向上查找的级别数
         */
        virtual void ReleaseExclusiveAtHigherLevel(int levels = 1)
        {
            EventDispatcher* target_dispatcher = this;
            
            // 向上查找到指定级别
            for (int i = 0; i < levels && target_dispatcher; i++)
            {
                target_dispatcher = target_dispatcher->GetParentDispatcher();
            }
            
            if (target_dispatcher && target_dispatcher->GetParentDispatcher())
            {
                target_dispatcher->GetParentDispatcher()->ExitExclusiveMode();
            }
        }

    protected:
        /**
         * 进入独占模式时调用
         */
        virtual void OnEnterExclusiveMode(EventDispatcher* exclusive_dispatcher) {}

        /**
         * 退出独占模式时调用
         */
        virtual void OnExitExclusiveMode(EventDispatcher* old_exclusive_dispatcher) {}

        /**
         * 分发事件给子分发器
         * 在独占模式下，只分发给独占分发器
         */
        template<typename EventType>
        void DispatchToChildren(const EventType& event)
        {
            if (is_exclusive_mode && exclusive_dispatcher)
            {
                // 独占模式下只分发给独占分发器
                exclusive_dispatcher->HandleEvent(&event);
            }
            else
            {
                // 正常模式下分发给所有子分发器
                for (EventDispatcher* child : child_dispatchers)
                {
                    if (child)
                    {
                        child->HandleEvent(&event);
                    }
                }
            }
        }

        /**
         * 处理事件（子类需要实现具体的事件处理）
         */
        virtual void HandleEvent(const void* event) {}
    };

} // namespace hgl::io