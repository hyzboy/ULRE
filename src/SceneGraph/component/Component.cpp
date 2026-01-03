#include<hgl/component/Component.h>
#include<hgl/graph/SceneNode.h>

COMPONENT_NAMESPACE_BEGIN

uint Component::unique_id_count=0;

Component::Component(ComponentDataPtr cdp,ComponentManager *cm,size_t derived_type_hash)
{
    unique_id=++unique_id_count;
    type_hash=derived_type_hash;    // 存储派生类的类型 hash

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

    if(hgl::GetTypeHash(cdp)!=hgl::GetTypeHash(Data))        //类型不对
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

U8String Component::GetComponentInfo() const
{
    return U8_TEXT("Component(unique_id: ") + U8String::numberOf(unique_id) + U8_TEXT(")");
}

COMPONENT_NAMESPACE_END
