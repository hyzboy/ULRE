#include<hgl/ecs/core/ComponentRow.h>
#include<hgl/ecs/core/Entity.h>

namespace hgl
{
    namespace ecs
    {
        EntityID ComponentRow::GetOwnerID() const
        {
            return owner ? owner->GetEntityID() : EntityID();
        }
    }//namespace ecs
}//namespace hgl
