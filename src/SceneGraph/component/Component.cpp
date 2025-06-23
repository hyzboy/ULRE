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

    bool Component::ChangeData(ComponentDataPtr cdp)
    {
        if(!cdp)
        {
            LOG_ERROR(OS_TEXT("Component::ChangeData: invalid component data pointer."));
            return(false);
        }

        if(Data==cdp)
            return(true); //数据没有变化

        ComponentData *cd=cdp.get();

        if(cd->GetHashCode()!=GetHashCode())        //类型不对
        {
            LOG_ERROR(OS_TEXT("Component::ChangeData: component data type mismatch."));
            return(false);
        }

        Data=cdp;
        return(true);
    }

    Component *Component::Duplication()
    {
        return GetManager()->CreateComponent(Data);
    }
}//namespace hgl::graph
