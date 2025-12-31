#include<hgl/ecs/BoundingBoxComponent.h>

namespace hgl
{
    namespace ecs
    {
        // Static member initialization
        std::shared_ptr<BoundingBoxDataStorage> BoundingBoxComponent::sharedStorage = nullptr;
    }//namespace ecs
}//namespace hgl
