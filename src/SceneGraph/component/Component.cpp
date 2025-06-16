#include<hgl/component/Component.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/log/LogInfo.h>

namespace hgl::graph
{
    uint Component::unique_id_count=0;

    Component::Component(ComponentDataPtr cdp,ComponentManager *cm)
    {
        unique_id=++unique_id_count;

        OwnerNode=nullptr;
        Data=cdp;
        Manager=cm;

        Manager->AttachComponent(this);

        LOG_INFO(AnsiString("Component::Component ")+AnsiString::numberOf(unique_id)+AnsiString(" ")+ToHexString<char>(this));
    }

    Component::~Component()
    {
        LOG_INFO(AnsiString("Component::~Component ")+AnsiString::numberOf(unique_id)+AnsiString(" ")+ToHexString<char>(this));

        if(OwnerNode)
        {
            OwnerNode->DetachComponent(this);
            OwnerNode=nullptr;
        }

        if(Manager)
            Manager->DetachComponent(this);
    }

    Component *Component::Duplication()
    {
        return GetManager()->CreateComponent(Data);
    }
}//namespace hgl::graph
