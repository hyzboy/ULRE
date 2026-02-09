#include<hgl/ecs/System.h>

namespace hgl
{
    namespace ecs
    {
        System::System(const std::string& name)
            : Object(name)
            , initialized(false)
            , systemType(SystemType::Unknown)
            , executionOrder(0)
        {
        }
    }//namespace ecs
}//namespace hgl
