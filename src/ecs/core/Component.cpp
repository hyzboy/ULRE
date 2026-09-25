#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/core/Context.h>

namespace hgl
{
    namespace ecs
    {
        Component::Component(const std::string& name)
            : componentName(name)
            , owner_id()
            , owner_context(nullptr)
            , owner_entity(nullptr)
            , version(0)
            , change_mask(0)
        {
        }

        Entity* Component::GetOwner() const
        {
            if (owner_entity)
                return owner_entity;

            if (!owner_context || !owner_id.IsValid())
                return nullptr;

            return owner_context->GetEntity(owner_id);
        }
    }//namespace ecs
}//namespace hgl

