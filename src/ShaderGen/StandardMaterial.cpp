#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN

StdMaterial::StdMaterial(const MaterialCreateConfig *mcc)
{
    mci=new MaterialCreateInfo(mcc);
}

MaterialCreateInfo *StdMaterial::Create(const VulkanDevAttr *dev_attr)
{
    mci->SetDevice(dev_attr);

    if(!BeginCustomShader())
        return(nullptr);

    if(mci->hasVertex())
        if(!CustomVertexShader(mci->GetVS()))
            return(nullptr);

    if(mci->hasGeometry())
        if(!CustomGeometryShader(mci->GetGS()))
            return(nullptr);

    if(mci->hasFragment())
        if(!CustomFragmentShader(mci->GetFS()))
            return(nullptr);

    if(!EndCustomShader())
        return(nullptr);

    if(!mci->CreateShader())
        return(nullptr);

    return(mci);
}

STD_MTL_NAMESPACE_END
