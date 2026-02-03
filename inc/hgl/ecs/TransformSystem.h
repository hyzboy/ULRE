#pragma once

#include<hgl/ecs/System.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/TransformComponent.h>
#include<vector>
#include<memory>

namespace hgl::ecs
{
    /**
     * TransformSystem
     *
     * Centralized update for TransformComponent.
        * - Updates dirty movable transforms per tick
        * - Static transforms are updated only on explicit call
     */
    class TransformSystem : public System
    {
    private:

        ECSContext* world = nullptr;
        bool updateMovable = true;

    public:

        TransformSystem(const std::string& name = "TransformSystem");
        ~TransformSystem() override = default;

    public:

        void SetWorld(ECSContext* w) { world = w; }
        void SetUpdateMovable(bool enabled) { updateMovable = enabled; }
        bool IsUpdateMovableEnabled() const { return updateMovable; }

        void Update(float deltaTime) override;
        void UpdateStaticDirty();

    private:

        void UpdateStaticTransformRecursive(const std::shared_ptr<TransformComponent>& comp);
    };
}//namespace hgl::ecs
