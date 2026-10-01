#include<hgl/ecs/components/ShadowComponent.h>

namespace hgl::ecs
{
    ShadowComponent::ShadowComponent(const std::string &name)
        : Component(name)
        , cast_shadow(true)
        , max_cast_distance(0.0f)
        , receive_shadow(true)
        , bias_multiplier(1.0f)
    {
    }
}//namespace hgl::ecs
