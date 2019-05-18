#ifndef HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
#define HGL_GRAPH_VULKAN_TEXTURE_INCLUDE

#include<hgl/graph/vulkan/VK.h>
VK_NAMESPACE_BEGIN
struct TextureData
{
    VkImage image;
    VkImageLayout image_layout;
    ImageView *image_view;
    uint32 mip_levels;
    bool linear;
};//struct TextureData

class Texture
{
protected:

    VkDevice device;
    TextureData *data;

public:

    operator VkImage        (){return data?data->image:nullptr;}
    operator VkImageLayout  (){return data?data->image_layout:VK_IMAGE_LAYOUT_UNDEFINED;}
    operator VkImageView    (){return data?*(data->image_view):nullptr;}

    const uint32    GetMipLevels()const{return data?data->mip_levels:0;}
    const bool      IsLinear    ()const{return data?data->linear:false;}

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
    uint32_t width,height;

public:

    Texture2D(uint32_t w,uint32_t h,VkDevice dev,TextureData *td):width(w),height(h),Texture(dev,td){}
    ~Texture2D()=default;
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
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
