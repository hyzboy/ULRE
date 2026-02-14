#pragma once

#include<hgl/ecs/core/Component.h>

namespace hgl::ecs
{
    class VisibilityDataStorage;

    /**
     * VisibilityComponent - Controls entity visibility in rendering
     * 
     * Simple component that marks whether an entity should be rendered.
     * Automatically updates VisibilityDataStorage when visibility changes.
     */
    class VisibilityComponent : public Component
    {
    private:
        bool visible = true;
        VisibilityDataStorage* storage = nullptr;

    public:

        explicit VisibilityComponent(const std::string& name = "Visibility")
            : Component(name)
        {
        }

        virtual ~VisibilityComponent();

        /// Set storage for automatic updates (called by system)
        void SetStorage(VisibilityDataStorage* s) { storage = s; }

        /// Set visibility state (automatically updates storage)
        void SetVisible(bool v);

        /// Get visibility state
        bool IsVisible() const { return visible; }
    };
}//namespace hgl::ecs

