﻿#include<iostream>
#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKInstance.h>

using namespace hgl;
using namespace hgl::graph;

VK_NAMESPACE_USING;

constexpr char *texture_compress_name[]=
{
    "NONE",
    "S3TC",
    "PVRTC",
    "ETC1",
    "ETC2",
    "EAC",
    "ATC",
    "ASTC",
    "YUV"
};

constexpr char *data_type_name[]
{
    "NONE",
    "UNORM",
    "SNORM",
    "USCALED",
    "SSCALED",
    "UINT",
    "SINT",
    "UFLOAT",
    "SFLOAT",
    "SRGB"
};//

vulkan::VulkanInstance *InitVulkanInstance()
{
    #ifdef _DEBUG
        if(!CheckStrideBytesByFormat())
        {
            std::cerr<<"check stride bytes by format failed."<<std::endl;
            return(nullptr);
        }
    #endif//_DEBUG

    InitNativeWindowSystem();

    InitVulkanProperties();

    CreateInstanceLayerInfo cili;

    memset(&cili,0,sizeof(CreateInstanceLayerInfo));

    cili.lunarg.standard_validation=true;
    cili.khronos.validation=true;

    return vulkan::CreateInstance("VulkanTest",nullptr,&cili);
}

int main(int,char **)
{
                    Window *        win             =nullptr;
            vulkan::VulkanInstance *      inst            =nullptr;
            vulkan::GPUDevice *        device          =nullptr;
    const   vulkan::GPUPhysicalDevice *physical_device =nullptr;

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
        std::cout<<i<<". ";
        
        std::cout<<" Format [ID:"<<vf->format<<"]["<<vf->name<<"] ";

        if(vf->compress_type!=TextureCompressType::NONE)
            std::cout<<"use "<<texture_compress_name[size_t(vf->compress_type)]<<" compress.";
        else
            std::cout<<vf->bytes<<" bytes/pixel.";

        if(vf->depth!=VulkanDataType::NONE)
            std::cout<<"[Depth:"<<data_type_name[size_t(vf->depth)]<<"]";
        
        if(vf->stencil!=VulkanDataType::NONE)
            std::cout<<"[Stencil:"<<data_type_name[size_t(vf->stencil)]<<"]";

        if((vf->depth==VulkanDataType::NONE)
         &&(vf->stencil==VulkanDataType::NONE))
            std::cout<<"[Color:"<<data_type_name[size_t(vf->color)]<<"]";

        {
            const VkFormatProperties fp=physical_device->GetFormatProperties(vf->format);

            if(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
                std::cout<<"[Optimal]";
            if(fp.linearTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
                std::cout<<"[Linear]";
            if(fp.bufferFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)
                std::cout<<"[Buffer]";
        }
            
        std::cout<<std::endl;

        ++vf;
    }

    delete inst;
 
    return 0;
}