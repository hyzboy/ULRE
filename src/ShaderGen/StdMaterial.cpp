#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

namespace hgl::graph::mtl{

StdMaterial::StdMaterial(const MaterialCreateConfig *mcc)
{
    mci=new MaterialCreateInfo(mcc);
}

MaterialCreateInfo *StdMaterial::Create(const contract::PhysicalDeviceProfileLite *profile)
{
    if(profile)
        mci->SetDevice(profile);

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

}//namespace hgl::graph::mtl
