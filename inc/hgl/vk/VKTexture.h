#ifndef HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
#define HGL_GRAPH_VULKAN_TEXTURE_INCLUDE

#include<hgl/vk/VK.h>
#include<hgl/vk/buffer/BufferMemory.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/graph/data/BitmapData.h>
#include<hgl/type/String.h>
#include<hgl/vk/VKTextureCreateInfo.h>
namespace hgl::graph{

BitmapData *LoadBitmapFromFile(const OSString &filename);

using TextureID=int;
class TextureManager;

class Texture
{
protected:

    TextureManager *manager;
    TextureID texture_id;

    TextureData *data;

public:

    TextureManager *            GetManager          ()      {return manager;}
    const TextureID             GetID               ()const noexcept {return texture_id;}

    TextureData *               GetData             ()      {return data;}

    VkDeviceMemory              GetDeviceMemory     ()      {return data?(data->memory?data->memory->operator VkDeviceMemory():VK_NULL_HANDLE):VK_NULL_HANDLE;}
    VkImage                     GetImage            ()      {return data?data->image:VK_NULL_HANDLE;}
    VkImageLayout               GetImageLayout      ()      {return data?data->image_layout:VK_IMAGE_LAYOUT_UNDEFINED;}
    void                        SetImageLayout      (VkImageLayout layout) {if(data) data->image_layout = layout;}
    VkImageView                 GetVulkanImageView  ()      {return data?data->image_view->GetImageView():VK_NULL_HANDLE;}

    DeviceMemory *              GetMemory           ()      {return data?data->memory:nullptr;}
    ImageView *                 GetImageView        ()      {return data?data->image_view:nullptr;}

    const uint32                GetMipLevel         ()const {return data?data->miplevel:0;}
    const bool                  IsOptimal           ()const {return data?data->tiling==VK_IMAGE_TILING_OPTIMAL:false;}
    const bool                  IsLinear            ()const {return data?data->tiling==VK_IMAGE_TILING_LINEAR:false;}

    const VkFormat              GetFormat           ()const {return data?data->image_view->GetFormat():VK_FORMAT_UNDEFINED;}
    const VkImageAspectFlags    GetAspect           ()const {return data?data->image_view->GetAspectFlags():0;}
    const VkExtent3D *          GetExtent           ()const {return data?&data->image_view->GetExtent():nullptr;}

    /**
     * 返回 bindless 采样用 image view。
     * 默认即主 view；2D 纹理覆写为单层 2D_ARRAY companion view。
     */
    virtual VkImageView         GetBindlessArrayView ()       {return GetVulkanImageView();}

    /**
     * 返回 bindless Cubemap 采样用 image view（textureCubeArray[]）。
     * 仅 TextureCube 覆写返回 CUBE_ARRAY companion view；其余纹理返回空句柄，
     * BindlessTextureManager 据此分流到不同 binding。
     */
    virtual VkImageView         GetBindlessCubeView  ()       {return VK_NULL_HANDLE;}

public:

    Texture(TextureManager *tm,const TextureID &id,TextureData *td)
    {
        manager=tm;
        texture_id=id;
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

    using Texture::Texture;
    virtual ~Texture2D()=default;

    static VkImageViewType GetImageViewType(){return VK_IMAGE_VIEW_TYPE_2D;}

    const uint32_t GetWidth ()const{return data?data->image_view->GetExtent().width:0;}
    const uint32_t GetHeight()const{return data?data->image_view->GetExtent().height:0;}

    /**
     * 惰性创建单层 2D_ARRAY companion view，供 bindless sampler2DArray[] 采样。
     * 主 view 保持 2D 类型，不影响 RTV/DSV。
     */
    VkImageView GetBindlessArrayView() override;
};//class Texture2D:public Texture

class Texture2DArray:public Texture
{
public:

    using Texture::Texture;
    virtual ~Texture2DArray()=default;

    static VkImageViewType GetImageViewType(){return VK_IMAGE_VIEW_TYPE_2D_ARRAY;}

    const uint32_t GetWidth ()const{return data?data->image_view->GetExtent().width:0;}
    const uint32_t GetHeight()const{return data?data->image_view->GetExtent().height:0;}
    const uint32_t GetLayer ()const{return data?data->image_view->GetExtent().depth:0;}
};//class Texture2DArray:public Texture

//class Texture3D:public Texture
//{
//    uint32_t width,height,depth;
//};//class Texture3D:public Texture

class TextureCube:public Texture
{
public:

    using Texture::Texture;
    ~TextureCube()=default;

    static VkImageViewType GetImageViewType(){return VK_IMAGE_VIEW_TYPE_CUBE;}

    const uint32_t GetWidth ()const{return data?data->image_view->GetExtent().width:0;}
    const uint32_t GetHeight()const{return data?data->image_view->GetExtent().height:0;}

    /**
     * 惰性创建 6 层 CUBE_ARRAY companion view（单张 Cubemap = DEPTH 1 的
     * CubeArray），供 bindless textureCubeArray[] 采样——
     * 与 Texture2D 的 2D_ARRAY companion view 模式完全一致。
     */
    VkImageView GetBindlessArrayView() override;

    /// Cube 纹理：bindless 写入走 binding=2(textureCubeArray)
    VkImageView GetBindlessCubeView() override {return GetBindlessArrayView();}
};//class TextureCube:public Texture

//class TextureCubeArray:public Texture
//{
//    uint32_t width,height,count;
//};//class TextureCubeArray:public Texture

}//namespace hgl::graph
#endif//HGL_GRAPH_VULKAN_TEXTURE_INCLUDE
