#pragma once

#include<cstdint>
#include<hgl/type/String.h>

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
            AnsiString objectName;

        private:

            static ObjectID nextObjectId;

        public:

            explicit Object(const char* name) : objectId(nextObjectId++), objectName(name) {}
            explicit Object(const AnsiString& name = "Object");
            explicit Object(const std::string& name) : objectId(nextObjectId++), objectName(name.c_str()) {}
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
            const AnsiString& GetName() const { return objectName; }

            /// Set object name
            void SetName(const AnsiString& name) { objectName = name; }

        public: // Lifecycle methods
        // 生命周期动词三套体系（W7 评估：不同阶段，非重复概念——勿统一改名）：
        //   Component::OnAttach/OnDetach          —— 组件挂载/卸载
        //   System::Initialize/Update/Shutdown    —— 系统初始化/运行
        //（原 Object::OnUpdate 每帧更新链已删除——逻辑更新归 tick 系统，
        //  不归组件；见 ECSContext::Tick 注释）
        };
    }//namespace ecs
}//namespace hgl
