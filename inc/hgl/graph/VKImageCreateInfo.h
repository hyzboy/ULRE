#pragma once
#include<hgl/graph/VKStruct.h>

VK_NAMESPACE_BEGIN
struct ImageCreateInfo:public vkstruct_flag<VkImageCreateInfo,VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO>
{
public:

    ImageCreateInfo(const VkImageCreateFlags f,const uint32_t &ml,const uint32_t &u,const uint32_t &t)
    {
        this->sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        this->pNext                 = nullptr;
        this->flags                 = f;
        this->mipLevels             = ml;
        this->samples               = VK_SAMPLE_COUNT_1_BIT;
        this->tiling                = VkImageTiling(t);
        this->usage                 = VkImageUsageFlags(u);
        this->sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
        this->queueFamilyIndexCount = 0;
        this->pQueueFamilyIndices   = nullptr;
        this->initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    ImageCreateInfo(const uint32_t &u,const uint32_t &t):ImageCreateInfo(0,1,u,t){}
    ImageCreateInfo(const uint32_t &ml,const uint32_t &u,const uint32_t &t):ImageCreateInfo(0,ml,u,t){}
    virtual ~ImageCreateInfo()=default;
};

struct Image1DCreateInfo:public ImageCreateInfo
{
public:

    Image1DCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const uint32_t &length,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_1D;
        this->extent.width  =length;
        this->extent.height =1;
        this->extent.depth  =1;
        this->arrayLayers   =1;
    }

    ~Image1DCreateInfo()=default;
};//

struct Image1DArrayCreateInfo:public ImageCreateInfo
{
public:

    Image1DArrayCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const uint32_t length,const uint32_t layers,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_1D;
        this->extent.width  =length;
        this->extent.height =1;
        this->extent.depth  =1;
        this->arrayLayers   =layers;
    }

    ~Image1DArrayCreateInfo()=default;
};//

struct Image2DCreateInfo:public ImageCreateInfo
{
public:

    Image2DCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const uint32_t w,const uint32_t h,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =w;
        this->extent.height =h;
        this->extent.depth  =1;
        this->arrayLayers   =1;
    }

    Image2DCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const VkExtent3D &extent,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =extent.width;
        this->extent.height =extent.height;
        this->extent.depth  =1;
        this->arrayLayers   =1;
    }

    ~Image2DCreateInfo()=default;
};//

struct Image2DArrayCreateInfo:public ImageCreateInfo
{
public:

    Image2DArrayCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const uint32_t w,const uint32_t h,const uint32_t layers,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =w;
        this->extent.height =h;
        this->extent.depth  =1;
        this->arrayLayers   =layers;
    }

    Image2DArrayCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const VkExtent3D &extent,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =extent.width;
        this->extent.height =extent.height;
        this->extent.depth  =1;
        this->arrayLayers   =extent.depth;
    }

    ~Image2DArrayCreateInfo()=default;
};//

struct Image3DCreateInfo:public ImageCreateInfo
{
public:

    Image3DCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const VkExtent3D &extent,const uint32_t &ml):ImageCreateInfo(ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_3D;
        this->extent        =extent;
        this->arrayLayers   =1;
    }

    ~Image3DCreateInfo()=default;
};//

struct ImageCubeCreateInfo:public ImageCreateInfo
{
public:

    ImageCubeCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const uint32_t w,const uint32_t h,const uint32_t &ml):ImageCreateInfo(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =w;
        this->extent.height =h;
        this->extent.depth  =1;
        this->arrayLayers   =6;
    }

    ImageCubeCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const VkExtent3D &extent,const uint32_t &ml):ImageCreateInfo(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =extent.width;
        this->extent.height =extent.height;
        this->extent.depth  =1;
        this->arrayLayers   =6;
    }

    ~ImageCubeCreateInfo()=default;
};//

struct ImageCubeArrayCreateInfo:public ImageCreateInfo
{
public:

    ImageCubeArrayCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const uint32_t w,const uint32_t h,const uint32 c,const uint32_t &ml):ImageCreateInfo(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =w;
        this->extent.height =h;
        this->extent.depth  =1;
        this->arrayLayers   =6*c;
    }

    ImageCubeArrayCreateInfo(const uint32_t &u,const uint32_t &t,const VkFormat &fmt,const VkExtent3D &extent,const uint32_t &ml):ImageCreateInfo(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,ml,u,t)
    {
        this->format        =fmt;
        this->imageType     =VK_IMAGE_TYPE_2D;
        this->extent.width  =extent.width;
        this->extent.height =extent.height;
        this->extent.depth  =1;
        this->arrayLayers   =6*extent.depth;
    }

    ~ImageCubeArrayCreateInfo()=default;
};//
VK_NAMESPACE_END
