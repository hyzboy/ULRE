#ifndef HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
#define HGL_GRAPH_VULKAN_TEXTURE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKMemory.h>
#include<hgl/graph/vulkan/VKImageView.h>
VK_NAMESPACE_BEGIN
struct TextureData
{
    Memory *            memory      =nullptr;
    VkImage             image       =VK_NULL_HANDLE;
    VkImageLayout       image_layout=VK_IMAGE_LAYOUT_UNDEFINED;
    ImageView *         image_view  =nullptr;
    uint32              mip_levels  =0;
    bool                linear      =false;
};//struct TextureData

class Texture
{
protected:

    VkDevice device;
    TextureData *data;

public:

    TextureData *               GetData             (){return data;}

    VkDeviceMemory              GetDeviceMemory     (){return data?data->memory->operator VkDeviceMemory():VK_NULL_HANDLE;}
    VkImage                     GetImage            (){return data?data->image:VK_NULL_HANDLE;}
    VkImageLayout               GetImageLayout      (){return data?data->image_layout:VK_IMAGE_LAYOUT_UNDEFINED;}
    VkImageView                 GetVulkanImageView  (){return data?data->image_view->operator VkImageView():VK_NULL_HANDLE;}

    Memory *                    GetMemory           (){return data?data->memory:nullptr;}
    ImageView *                 GetImageView        (){return data?data->image_view:nullptr;}

    const uint32                GetMipLevels()const{return data?data->mip_levels:0;}
    const bool                  IsLinear    ()const{return data?data->linear:false;}

    const VkFormat              GetFormat   ()const{return data?data->image_view->GetFormat():VK_FORMAT_UNDEFINED;}
    const VkImageAspectFlags    GetAspect   ()const{return data?data->image_view->GetAspectFlags():0;}
    const VkExtent3D *          GetExtent   ()const{return data?&data->image_view->GetExtent():nullptr;}

public:

    Texture(VkDevice dev,TextureData *td)
    {
        device=dev;
        data=td;
    }

    virtual ~Texture();
};//class Texture

//class Texture1D:public Texture
//{
//    uint32_t length;
//};//class Texture1D:public Texture

//class Texture1DArray:public Texture
//{
//    uint32_t length,count;
//};//class Texture1DArray:public Texture

class Texture2D:public Texture
{
public:

    Texture2D(VkDevice dev,TextureData *td):Texture(dev,td){}
    ~Texture2D()=default;

    const uint32_t GetWidth()const{return data?data->image_view->GetExtent().width:0;}
    const uint32_t GetHeight()const{return data?data->image_view->GetExtent().height:0;}
};//class Texture2D:public Texture

//class Texture2DArray:public Texture
//{
//    uint32_t width,height,count;
//};//class Texture2DArray:public Texture

//class Texture3D:public Texture
//{
//    uint32_t width,height,depth;
//};//class Texture3D:public Texture

//class TextureCubemap:public Texture
//{
//    uint32_t width,height;
//};//class TextureCubemap:public Texture

//class TextureCubemapArray:public Texture
//{
//    uint32_t width,height,count;
//};//class TextureCubemapArray:public Texture

Texture2D *CreateTexture2D(VkDevice device,VkFormat format,uint32_t width,uint32_t height,VkImageAspectFlagBits aspectMask,VkImage image,VkImageLayout image_layout);
Texture2D *CreateTexture2D(VkDevice device,const PhysicalDevice *pd,const VkFormat format,uint32_t width,uint32_t height,const VkImageAspectFlags aspectMask,const uint usage,const VkImageLayout image_layout,const VkImageTiling tiling);

VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
