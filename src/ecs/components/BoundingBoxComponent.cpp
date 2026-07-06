#include<hgl/ecs/components/BoundingBoxComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<array>

namespace hgl
{
    namespace ecs
    {
        // Static member initialization
        std::shared_ptr<BoundingBoxDataStorage> BoundingBoxComponent::sharedStorage = nullptr;
    }//namespace ecs
}//namespace hgl


