#include<hgl/ecs/Object.h>

namespace hgl
{
    namespace ecs
    {
        Object::ObjectID Object::nextObjectId = 1;

        Object::Object(const std::string& name)
            : objectId(nextObjectId++)
            , objectName(name)
        {
        }
    }//namespace ecs
}//namespace hgl
