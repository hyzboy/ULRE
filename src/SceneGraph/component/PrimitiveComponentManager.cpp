#include<hgl/component/PrimitiveComponent.h>
#include<hgl/graph/mesh/Primitive.h>

COMPONENT_NAMESPACE_BEGIN

//uint PrimitiveComponentData::unique_id_count=0;

PrimitiveComponentData::~PrimitiveComponentData()
{
//    LogInfo(AnsiString("~PrimitiveComponentData():")+AnsiString::numberOf(unique_id));

    if(primitive)
    {
        //primitive->Release();      //外面有PrimitiveManager管理，不需要在这里释放.但未来要考虑是增加Release函数通知这里释放了一次使用权
        primitive=nullptr;
    }
}

Component *PrimitiveComponentManager::CreateComponent(ComponentDataPtr cdp)
{
    if(!cdp)return(nullptr);

    if(!dynamic_cast<PrimitiveComponentData *>(cdp.get()))
    {
        //LogError(OS_TEXT("PrimitiveComponentManager::CreatePrimitiveComponent: invalid component data type."));
        return(nullptr);
    }

    return(new PrimitiveComponent(cdp,this));
}

Component *PrimitiveComponentManager::CreateComponent(Primitive *m)
{
    ComponentDataPtr cdp=new PrimitiveComponentData(m);

    return CreateComponent(cdp);
}

U8String RenderComponent::GetComponentInfo() const
{
    return U8_TEXT("RenderComponent(unique_id: ") + U8String::numberOf(GetUniqueID()) + U8_TEXT(")");
}

U8String PrimitiveComponent::GetComponentInfo() const
{
    U8String info = U8_TEXT("PrimitiveComponent(unique_id: ") + U8String::numberOf(GetUniqueID());
    
    Primitive *prim = GetPrimitive();
    if(prim)
    {
        info += U8_TEXT(", primitive: ") + ToU8String(prim->GetGeometryName());
    }
    
    if(override_material)
    {
        info += U8_TEXT(", override_material: ") + ToU8String(override_material->GetMaterial()->GetName());
    }

    info += U8_TEXT(")");
    return info;
}

COMPONENT_NAMESPACE_END
