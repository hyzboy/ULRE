#include <hgl/ecs/components/AssetInstanceComponent.h>

// AssetInstanceComponent is header-only; this .cpp exists only so CMake
// includes the translation unit, enabling the component to be picked up
// by the ECS component registry at link time if needed.

namespace hgl::ecs
{
    // No out-of-line definitions needed.
}//namespace hgl::ecs
