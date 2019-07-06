#include<hgl/graph/vulkan/VKTexture.h>
#include<hgl/graph/vulkan/VKImageView.h>
#include<hgl/graph/vulkan/VKMemory.h>
VK_NAMESPACE_BEGIN
Texture::~Texture()
{
    if(!data)return;

    if(data->image_view)
        delete data->image_view;

    if(data->memory)        //没有memory的纹理都是从其它地方借来的，所以就不存在删除
    {
        delete data->memory;

        if(data->image)
            vkDestroyImage(device,data->image,nullptr);
    }
}

Texture2D *CreateTexture2D(VkDevice device,VkFormat format,uint32_t width,uint32_t height,VkImageAspectFlagBits aspectMask,VkImage image,VkImageLayout image_layout)
{
    TextureData *tex_data=new TextureData;

    tex_data->memory        =nullptr;
    tex_data->image         =image;
    tex_data->image_layout  =image_layout;
    tex_data->image_view    =CreateImageView(device,VK_IMAGE_VIEW_TYPE_2D,format,aspectMask,image);

    tex_data->mip_levels    =0;
    tex_data->linear        =false;
    tex_data->format        =format;
    tex_data->aspect        =aspectMask;

    tex_data->extent.width  =width;
    tex_data->extent.height =height;
    tex_data->extent.depth  =1;

    return(new Texture2D(width,height,device,tex_data));
}

Texture2D *CreateTexture2D(VkDevice device,const PhysicalDevice *pd,const VkFormat format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,const VkImageTiling tiling)
{
    if(format<VK_FORMAT_BEGIN_RANGE||format>VK_FORMAT_END_RANGE)return(nullptr);
    if(width<1||height<1)return(nullptr);
   
    VkImageCreateInfo imageCreateInfo;

    imageCreateInfo.sType                   = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.pNext                   = nullptr;
    imageCreateInfo.flags                   = 0;
    imageCreateInfo.imageType               = VK_IMAGE_TYPE_2D;
    imageCreateInfo.format                  = format;
    imageCreateInfo.extent.width            = width;
    imageCreateInfo.extent.height           = height;
    imageCreateInfo.extent.depth            = 1;
    imageCreateInfo.mipLevels               = 1;
    imageCreateInfo.arrayLayers             = 1;
    imageCreateInfo.samples                 = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.usage                   = usage;
    imageCreateInfo.sharingMode             = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.queueFamilyIndexCount   = 0;
    imageCreateInfo.pQueueFamilyIndices     = nullptr;
    imageCreateInfo.initialLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.tiling                  = tiling;

    VkImage image;

    if(vkCreateImage(device, &imageCreateInfo, nullptr, &image)!=VK_SUCCESS)
        return(nullptr);

    VkMemoryRequirements memReqs;

    vkGetImageMemoryRequirements(device, image, &memReqs);

    Memory *dm=CreateMemory(device,pd,memReqs,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
     
    if(dm&&dm->BindImage(image))
    {
        ImageView *image_view=CreateImageView2D(device,format,aspectMask,image);

        if(image_view)
        {
            TextureData *tex_data=new TextureData;

            tex_data->mip_levels    = 0;
            tex_data->memory        = dm;
            tex_data->image_layout  = image_layout;
            tex_data->image         = image;
            tex_data->image_view    = image_view;
            tex_data->format        = format;
            tex_data->aspect        = aspectMask;
            tex_data->extent        = imageCreateInfo.extent;
    
            return(new Texture2D(width,height,device,tex_data));
        }
    }

    delete dm;
    vkDestroyImage(device,image,nullptr);
    return(nullptr);
}
VK_NAMESPACE_END
