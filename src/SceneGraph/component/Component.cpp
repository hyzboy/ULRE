#include<hgl/component/Component.h>
#include<hgl/graph/SceneNode.h>

COMPONENT_NAMESPACE_BEGIN

uint Component::unique_id_count=0;

Component::Component(ComponentDataPtr cdp,ComponentManager *cm)
{
    unique_id=++unique_id_count;

    Log.SetLoggerInstanceName(OSString::numberOf(unique_id));

    OwnerNode=nullptr;
    Data=cdp;
    Manager=cm;

    Manager->AttachComponent(this);

    LogInfo(AnsiString("Component::Component ")+AnsiString::numberOf(unique_id)+AnsiString(" ")+ToHexString<char>(this));
}

Component::~Component()
{
    LogInfo(AnsiString("Component::~Component ")+AnsiString::numberOf(unique_id)+AnsiString(" ")+ToHexString<char>(this));

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
        LogError(OS_TEXT("Component::ChangeData: invalid component data pointer."));
        return(false);
    }

    if(Data==cdp)
        return(true); //数据没有变化

    if(cdp->GetTypeHash()!=GetTypeHash())        //类型不对
    {
        LogError(OS_TEXT("Component::ChangeData: component data type mismatch."));
        return(false);
    }

    Data=cdp;
    return(true);
}

Component *Component::Clone()
{
    return GetManager()->CreateComponent(Data);
}

COMPONENT_NAMESPACE_END
