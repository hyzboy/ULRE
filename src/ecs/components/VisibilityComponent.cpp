#include<hgl/ecs/components/VisibilityComponent.h>
#include<hgl/ecs/support/VisibilityDataStorage.h>

namespace hgl::ecs
{
    VisibilityComponent::~VisibilityComponent()
    {
        // Remove from storage when component is destroyed
        if (storage && !visible)
        {
            storage->SetVisible(GetOwnerID());
        }
    }

    void VisibilityComponent::SetVisible(bool v)
    {
        if (visible == v)
            return;

        visible = v;

        // Update storage
        if (storage)
        {
            if (visible)
            {
                storage->SetVisible(GetOwnerID());
            }
            else
            {
                storage->SetInvisible(GetOwnerID());
            }
        }
    }
}//namespace hgl::ecs

