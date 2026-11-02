#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/ecs/core/entity/Entity.h>

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
