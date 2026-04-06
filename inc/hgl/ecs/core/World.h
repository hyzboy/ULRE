#pragma once

#include<hgl/ecs/core/Object.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/log/Log.h>
#include<memory>
#include<vector>

namespace hgl
{
    namespace graph
    {
        class RenderCmdBuffer;
    }

    namespace ecs
    {
        /**
         * World - 场景级编排层
         *
         * 职责：
         * - 持有并驱动 ECSContext
         * - 管理父驱动子的 World 层级刷新
         * - 提供 Tick/Render 重入保护
         */
        class World final : public Object
        {
        private:

            OBJECT_LOGGER

            std::shared_ptr<ECSContext> context;
            std::vector<std::shared_ptr<World>> children;

            bool active = true;
            bool is_ticking = false;
            bool is_rendering = false;

        public:

            explicit World(const std::string& name = "World");
            World(std::shared_ptr<ECSContext> ctx, const std::string& name = "World");
            ~World() override = default;

        public:

            ECSContext* GetContext() { return context.get(); }
            const ECSContext* GetContext() const { return context.get(); }

            void SetContext(const std::shared_ptr<ECSContext>& ctx);

            bool IsActive() const { return active; }
            void SetActive(bool value) { active = value; }

            bool IsTicking() const { return is_ticking; }
            bool IsRendering() const { return is_rendering; }

        public:

            void Initialize();
            void Shutdown();

            void Tick(float delta_time);
            void Render(graph::RenderCmdBuffer* cmd, float delta_time);

        public:

            void AddChild(const std::shared_ptr<World>& child);
            bool RemoveChild(World* child);
            void ClearChildren();

            const std::vector<std::shared_ptr<World>>& GetChildren() const { return children; }

        public:

            template<typename T = Entity, typename... Args>
            T* CreateEntity(Args&&... args)
            {
                if (!context)
                    return nullptr;

                return context->CreateEntity<T>(std::forward<Args>(args)...);
            }

            Entity* GetEntity(EntityID id)
            {
                if (!context)
                    return nullptr;

                return context->GetEntity(id);
            }

            const Entity* GetEntity(EntityID id) const
            {
                if (!context)
                    return nullptr;

                return context->GetEntity(id);
            }

            void DestroyEntity(EntityID id)
            {
                if (!context)
                    return;

                context->DestroyEntity(id);
            }

            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterTickSystem(Args&&... args)
            {
                if (!context)
                    return nullptr;

                return context->RegisterTickSystem<T>(std::forward<Args>(args)...);
            }

            template<typename T, typename... Args>
            std::shared_ptr<T> RegisterRenderSystem(Args&&... args)
            {
                if (!context)
                    return nullptr;

                return context->RegisterRenderSystem<T>(std::forward<Args>(args)...);
            }

            template<typename T>
            std::shared_ptr<T> GetSystem() const
            {
                if (!context)
                    return nullptr;

                return context->GetSystem<T>();
            }
        };
    }//namespace ecs
}//namespace hgl
