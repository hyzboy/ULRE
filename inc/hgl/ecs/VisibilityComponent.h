#pragma once

#include<hgl/ecs/Component.h>

namespace hgl::ecs
{
    /**
     * VisibilityComponent - Controls entity visibility
     * 
     * Used to control whether an entity and its components should be rendered/active.
     * Rendering systems check this component to determine visibility.
     */
    class VisibilityComponent : public Component
    {
    private:
        bool visible = true;    ///< Visibility state

    public:

        explicit VisibilityComponent(const std::string& name = "Visibility")
            : Component(name)
        {
        }

        virtual ~VisibilityComponent() = default;

        /// Set visibility state
        void SetVisible(bool v) { visible = v; }

        /// Get visibility state
        bool IsVisible() const { return visible; }
    };
}//namespace hgl::ecs
