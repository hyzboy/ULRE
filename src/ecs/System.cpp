#include<hgl/ecs/System.h>

namespace hgl
{
    namespace ecs
    {
        System::System(const std::string& name)
            : Object(name)
            , initialized(false)
        {
        }
    }//namespace ecs
}//namespace hgl
