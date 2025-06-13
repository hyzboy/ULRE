#include<hgl/component/Component.h>
#include<hgl/graph/SceneNode.h>

namespace hgl::graph
{
    Component::Component(ComponentData *cd,ComponentManager *cm)
    {
        OwnerNode=nullptr;
        Data=cd;
        Manager=cm;

        Manager->AttachComponent(this);
    }

    Component::~Component()
    {
        if(OwnerNode)
        {
            OwnerNode->DetachComponent(this);
            OwnerNode=nullptr;
        }

        Manager->DetachComponent(this);

        SAFE_CLEAR(Data);
    }
}//namespace hgl::graph
