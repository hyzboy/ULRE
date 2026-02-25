#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/vk/VKDeviceAttribute.h>

namespace hgl::graph::mtl{

// Desktop-safe fallback limits used when no real device is available
static constexpr uint32_t kFallbackUBORange  = 65536;      // 64 KB — conservative minimum
static constexpr uint32_t kFallbackSSBORange = 0x7fffffff; // 2 GB — effectively unlimited

StdMaterial::StdMaterial(const MaterialCreateConfig *mcc)
{
    mci=new MaterialCreateInfo(mcc);
}

MaterialCreateInfo *StdMaterial::Create(const VulkanDevAttr *dev_attr)
{
    // When no device is provided, fall back to GLSL-only generation
    if(!dev_attr)
        return CreateGLSLOnly(nullptr);

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

MaterialCreateInfo *StdMaterial::CreateGLSLOnly(const VulkanDevAttr *dev_attr)
{
    if(dev_attr)
        mci->SetDevice(dev_attr);
    else
        mci->SetDeviceFallback(kFallbackUBORange, kFallbackSSBORange);

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

    if(!mci->BuildGLSLOnly())
        return(nullptr);

    return(mci);
}

}//namespace hgl::graph::mtl
