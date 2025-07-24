#ifndef HGL_GRAPH_EVENT_DISPATCHER_INCLUDE
#define HGL_GRAPH_EVENT_DISPATCHER_INCLUDE

#include<hgl/input/Event.h>
#include<hgl/type/List.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 事件调度器接口
         * 用于在渲染框架、场景和场景节点之间传递事件
         */
        class EventDispatcher
        {
        public:
            virtual ~EventDispatcher() = default;

            /**
             * 分发事件到所有监听器
             * @param event 输入事件
             * @return 是否有监听器处理了该事件
             */
            virtual bool DispatchEvent(const device_input::InputEvent& event) = 0;

            /**
             * 添加事件监听器
             * @param listener 事件监听器
             */
            virtual void AddEventListener(EventDispatcher* listener) = 0;

            /**
             * 移除事件监听器
             * @param listener 事件监听器
             */
            virtual void RemoveEventListener(EventDispatcher* listener) = 0;

        protected:
            /**
             * 处理事件的具体实现
             * @param event 输入事件
             * @return 是否处理了该事件
             */
            virtual bool HandleEvent(const device_input::InputEvent& event) = 0;
        };

        /**
         * 基础事件调度器实现
         */
        class BaseEventDispatcher : public EventDispatcher
        {
        protected:
            ObjectList<EventDispatcher> listeners;

        public:
            virtual ~BaseEventDispatcher() = default;

            bool DispatchEvent(const device_input::InputEvent& event) override
            {
                // 首先尝试自己处理事件
                if (HandleEvent(event))
                    return true;

                // 如果自己没有处理，则分发给监听器
                const int count = listeners.GetCount();
                EventDispatcher** listener_list = listeners.GetData();
                
                for (int i = 0; i < count; i++)
                {
                    if (listener_list[i]->DispatchEvent(event))
                        return true;
                }

                return false;
            }

            void AddEventListener(EventDispatcher* listener) override
            {
                if (listener)
                    listeners.Add(listener);
            }

            void RemoveEventListener(EventDispatcher* listener) override
            {
                if (listener)
                    listeners.Delete(listener);
            }

        protected:
            // 默认不处理任何事件，子类可以覆盖
            bool HandleEvent(const device_input::InputEvent& event) override
            {
                return false;
            }
        };
    }//namespace graph
}//namespace hgl

#endif//HGL_GRAPH_EVENT_DISPATCHER_INCLUDE