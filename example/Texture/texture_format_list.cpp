#include<iostream>
#include<iomanip>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInstance.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_USING;


VulkanInstance *InitVulkanInstance()
{
    #ifdef _DEBUG
        if(!CheckStrideBytesByFormat())
        {
            std::cerr<<"check stride bytes by format failed."<<std::endl;
            return(nullptr);
        }
    #endif//_DEBUG

    InitNativeWindowSystem();

    //InitVulkanProperties();

    CreateInstanceLayerInfo cili;

    memset(&cili,0,sizeof(CreateInstanceLayerInfo));

    cili.lunarg.standard_validation=true;
    cili.khronos.validation=true;

    return CreateInstance("VulkanTest",nullptr,&cili);
}

int main(int,char **)
{
            Window *            win             =nullptr;
            VulkanInstance *    inst            =nullptr;
            VulkanDevice *         device          =nullptr;
    const   VulkanPhyDevice * physical_device =nullptr;

    inst=InitVulkanInstance();

    if(!inst)
    {
        std::cerr<<"[VULKAN FATAL ERROR] Create Vulkan Instance failed.";
        return(false);
    }

        physical_device=inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

    if(!physical_device)
        physical_device=inst->GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);

    uint32_t count;
    const VulkanFormat *vf=GetVulkanFormatList(count);

    if(!count)return(255);

    for(uint32_t i=0;i<count;i++)
    {
        std::cout<<std::setw(4)<<i<<". ";
        
        std::cout<<"Format [ID:"<<std::setw(10)<<vf->format<<"]["<<std::setw(16)<<vf->name<<"]";

        if(vf->depth!=VulkanBaseType::NONE)
            std::cout<<"[  Depth:"<<std::setw(8)<<VulkanBaseTypeName[size_t(vf->depth)]<<"]";
        
        if(vf->stencil!=VulkanBaseType::NONE)
            std::cout<<"[Stencil:"<<std::setw(8)<<VulkanBaseTypeName[size_t(vf->stencil)]<<"]";

        if((vf->depth==VulkanBaseType::NONE)
         &&(vf->stencil==VulkanBaseType::NONE))
            std::cout<<"[  Color:"<<std::setw(8)<<VulkanBaseTypeName[size_t(vf->color)]<<"]";

        {
            const VkFormatProperties fp=physical_device->GetFormatProperties(vf->format);

            char olb[]="[   ]";

            if(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT )olb[1]='O';
            if(fp.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT  )olb[2]='L';
            if(fp.bufferFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT        )olb[2]='B';

            std::cout<<olb;
        }

        if(vf->compress_type!=TextureCompressType::NONE)
            std::cout<<" use "<<TextureCompressTypeName[size_t(vf->compress_type)]<<" compress.";
        else
            std::cout<<std::setw(4)<<vf->bytes<<" bytes/pixel.";
            
        std::cout<<std::endl;

        ++vf;
    }

    delete inst;
 
    return 0;
}
