#include<hgl/graph/VKImageView.h>

VK_NAMESPACE_BEGIN
ImageView::~ImageView()
{
    vkDestroyImageView(device,image_view,nullptr);
}

ImageView *CreateImageView(VkDevice device,VkImageViewType type,VkFormat format,const VkExtent3D &ext,const uint32_t &miplevel,VkImageAspectFlags aspectMask,VkImage img)
{
    ImageViewCreateInfo iv_createinfo;

    iv_createinfo.image     =img;
    iv_createinfo.format    =format;
    iv_createinfo.viewType  =type;
    iv_createinfo.subresourceRange.aspectMask       =aspectMask;
    iv_createinfo.subresourceRange.baseMipLevel     =0;
    iv_createinfo.subresourceRange.levelCount       =miplevel;
    iv_createinfo.subresourceRange.baseArrayLayer   =0;

    if(type==VK_IMAGE_VIEW_TYPE_CUBE)
        iv_createinfo.subresourceRange.layerCount   =6;
    else
    if(type==VK_IMAGE_VIEW_TYPE_CUBE_ARRAY)
        iv_createinfo.subresourceRange.layerCount   =6*ext.depth;
    else
        iv_createinfo.subresourceRange.layerCount   =ext.depth;


    if(aspectMask&VK_IMAGE_ASPECT_DEPTH_BIT)
    {
        if(format>=VK_FORMAT_D16_UNORM_S8_UINT)
            iv_createinfo.subresourceRange.aspectMask|=VK_IMAGE_ASPECT_STENCIL_BIT;

        iv_createinfo.components.r=VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_createinfo.components.g=VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_createinfo.components.b=VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_createinfo.components.a=VK_COMPONENT_SWIZZLE_IDENTITY;
    }
    else
    {
        iv_createinfo.components.r=VK_COMPONENT_SWIZZLE_R;
        iv_createinfo.components.g=VK_COMPONENT_SWIZZLE_G;
        iv_createinfo.components.b=VK_COMPONENT_SWIZZLE_B;
        iv_createinfo.components.a=VK_COMPONENT_SWIZZLE_A;
    }

    VkImageView img_view;

    if(vkCreateImageView(device,&iv_createinfo,nullptr,&img_view)!=VK_SUCCESS)
        return(nullptr);

    return(new ImageView(device,img_view,type,format,ext,miplevel,aspectMask));
}
VK_NAMESPACE_END
