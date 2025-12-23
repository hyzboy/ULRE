#include<hgl/component/SceneComponent.h>

COMPONENT_NAMESPACE_BEGIN

U8String SceneComponent::GetComponentInfo() const
{
    return U8_TEXT("SceneComponent(unique_id: ") + U8String::numberOf(GetUniqueID()) + U8_TEXT(")");
}

COMPONENT_NAMESPACE_END
