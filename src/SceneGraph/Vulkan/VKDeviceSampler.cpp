#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKSampler.h>
VK_NAMESPACE_BEGIN
Sampler *GPUDevice::CreateSampler(VkSamplerCreateInfo *sci)
{
    static VkSamplerCreateInfo default_sampler_create_info=
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        false,
        1.0f,
        false,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,//VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false
    };

    if(!sci)
        sci=&default_sampler_create_info;

    VkSampler sampler;

    sci->sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    if(vkCreateSampler(attr->device,sci,nullptr,&sampler)!=VK_SUCCESS)
        return(nullptr);

    return(new Sampler(attr->device,sampler));
}

Sampler *GPUDevice::CreateSampler(Texture *tex)
{
    VkSamplerCreateInfo sci=
    {
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        nullptr,
        0,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,
        false,
        1.0f,
        false,
        VK_COMPARE_OP_NEVER,
        0.0f,
        0.0f,
        VK_BORDER_COLOR_INT_OPAQUE_BLACK,//VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false
    };

    VkSampler sampler;
    
    sci.anisotropyEnable = attr->physical_device->SupportSamplerAnisotropy();

    if(sci.anisotropyEnable)
        sci.maxAnisotropy = attr->physical_device->GetMaxSamplerAnisotropy();

    if(tex)
        sci.maxLod=tex->GetMipLevel();

    if(vkCreateSampler(attr->device,&sci,nullptr,&sampler)!=VK_SUCCESS)
        return(nullptr);

    return(new Sampler(attr->device,sampler));
}
VK_NAMESPACE_END
