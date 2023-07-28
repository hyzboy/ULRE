#ifndef HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
#define HGL_GRAPH_VULKAN_TEXTURE_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKMemory.h>
#include<hgl/graph/VKImageView.h>
#include<hgl/graph/BitmapData.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKTextureCreateInfo.h>
VK_NAMESPACE_BEGIN

BitmapData *LoadBitmapFromFile(const OSString &filename);

class Texture
{
protected:

    VkDevice device;
    TextureData *data;

public:

    TextureData *               GetData             ()      {return data;}

    VkDeviceMemory              GetDeviceMemory     ()      {return data?data->memory->operator VkDeviceMemory():VK_NULL_HANDLE;}
    VkImage                     GetImage            ()      {return data?data->image:VK_NULL_HANDLE;}
    VkImageLayout               GetImageLayout      ()      {return data?data->image_layout:VK_IMAGE_LAYOUT_UNDEFINED;}
    VkImageView                 GetVulkanImageView  ()      {return data?data->image_view->GetImageView():VK_NULL_HANDLE;}

    DeviceMemory *              GetMemory           ()      {return data?data->memory:nullptr;}
    ImageView *                 GetImageView        ()      {return data?data->image_view:nullptr;}

    const uint32                GetMipLevel         ()const {return data?data->miplevel:0;}
    const bool                  IsOptimal           ()const {return data?data->tiling==VK_IMAGE_TILING_OPTIMAL:false;}
    const bool                  IsLinear            ()const {return data?data->tiling==VK_IMAGE_TILING_LINEAR:false;}

    const VkFormat              GetFormat           ()const {return data?data->image_view->GetFormat():VK_FORMAT_UNDEFINED;}
    const VkImageAspectFlags    GetAspect           ()const {return data?data->image_view->GetAspectFlags():0;}
    const VkExtent3D *          GetExtent           ()const {return data?&data->image_view->GetExtent():nullptr;}

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

    static VkImageViewType GetImageViewType(){return VK_IMAGE_VIEW_TYPE_2D;}

    const uint32_t GetWidth ()const{return data?data->image_view->GetExtent().width:0;}
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

class TextureCube:public Texture
{
public:

    TextureCube(VkDevice dev,TextureData *td):Texture(dev,td){}
    ~TextureCube()=default;

    static VkImageViewType GetImageViewType(){return VK_IMAGE_VIEW_TYPE_CUBE;}

    const uint32_t GetWidth ()const{return data?data->image_view->GetExtent().width:0;}
    const uint32_t GetHeight()const{return data?data->image_view->GetExtent().height:0;}
};//class TextureCube:public Texture

//class TextureCubeArray:public Texture
//{
//    uint32_t width,height,count;
//};//class TextureCubeArray:public Texture

VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
