#pragma once

#include<cstdint>
#include<string>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        /**
         * Base class for all ECS objects
         * Provides fundamental object lifetime and identification
         */
        class Object
        {
        public:

            using ObjectID = uint64_t;

        protected:

            ObjectID objectId;
            std::string objectName;

        private:

            static ObjectID nextObjectId;

        public:

            explicit Object(const std::string& name = "Object");
            virtual ~Object() = default;

            // Delete copy operations
            Object(const Object&) = delete;
            Object& operator=(const Object&) = delete;

            // Allow move operations
            Object(Object&&) noexcept = default;
            Object& operator=(Object&&) noexcept = default;

        public:

            /// Get unique object ID
            ObjectID GetID() const { return objectId; }

            /// Get object name
            const std::string& GetName() const { return objectName; }

            /// Set object name
            void SetName(const std::string& name) { objectName = name; }

        public: // Lifecycle methods
        // 生命周期动词三套体系（W7 评估：不同阶段，非重复概念——勿统一改名）：
        //   Object::OnCreate/Update/Destroy      —— 对象创建/销毁
        //   Component::OnAttach/OnDetach         —— 组件挂载/卸载
        //   System::Initialize/Update/Shutdown   —— 系统初始化/运行

            /// Called when object is created
            virtual void OnCreate() {}

            /// Called every frame for updates
            virtual void OnUpdate(float) {}

            /// Called when object is destroyed
            virtual void OnDestroy() {}
        };
    }//namespace ecs
}//namespace hgl
