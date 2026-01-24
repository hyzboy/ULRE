#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/IDName.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/ImageRegion.h>
#include<hgl/graph/VKTexture.h>

VK_NAMESPACE_BEGIN

GRAPH_MODULE_CLASS(TextureManager)
{
    DeviceQueue *texture_queue=nullptr;
    TextureCmdBuffer *texture_cmd_buf=nullptr;
    
private:

    TextureID texture_serial=0;

    const TextureID AcquireID(){return texture_serial++;}                       ///<取得一个新的纹理ID

private:

    SortedSet<VkImage> image_set;
    SortedSet<Texture *> texture_set;                                           ///<纹理合集

    Map<TextureID,Texture *> texture_by_id;
    Map<OSString,Texture *> texture_by_filename;

    const TextureID Add(Texture *);
    const TextureID Add(Texture *,const OSString &);

public:

    TextureManager(RenderFramework *rf);
    virtual ~TextureManager();

    const VkFormatProperties GetFormatProperties(const VkFormat)const;

public:     //Buffer

    DeviceBuffer *CreateTransferSourceBuffer(const VkDeviceSize,const void *data_ptr=nullptr);

private:     //Image

    VkImage CreateImage         (VkImageCreateInfo *);
    void    DestroyImage        (VkImage);

private:    //texture

    bool CopyBufferToImage      (const CopyBufferToImageInfo *info,VkPipelineStageFlags destinationStage);

    bool CopyBufferToImage      (Texture *,DeviceBuffer *buf,const VkBufferImageCopy *,const int count,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags);//=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    bool CopyBufferToImage2D    (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic_list,const int bic_count,  VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic_list,   bic_count,  0,1,dstStage);}
    bool CopyBufferToImage2D    (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic,                           VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic,        1,          0,1,dstStage);}

    bool CopyBufferToImageCube  (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic_list,const int bic_count,  VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic_list,   bic_count,  0,6,dstStage);}
    bool CopyBufferToImageCube  (Texture *tex,DeviceBuffer *buf,const VkBufferImageCopy *bic,                           VkPipelineStageFlags dstStage){return CopyBufferToImage(tex,buf,bic,        1,          0,6,dstStage);}

    bool CommitTexture2D        (Texture2D *,DeviceBuffer *buf,VkPipelineStageFlags stage);
    bool CommitTexture2DMipmaps (Texture2D *,DeviceBuffer *buf,const VkExtent3D &,uint32_t);

    bool CommitTextureCube          (TextureCube *,DeviceBuffer *buf,const uint32_t mipmaps_zero_bytes,VkPipelineStageFlags stage);
    bool CommitTextureCubeMipmaps   (TextureCube *,DeviceBuffer *buf,const VkExtent3D &,uint32_t);

    bool SubmitTexture          (const VkCommandBuffer *cmd_bufs,const uint32_t count=1);           ///<提交纹理处理到队列

public: //Format

    bool CheckFormatSupport(const VkFormat,const uint32_t bits,ImageTiling tiling=ImageTiling::Optimal)const;

    bool CheckTextureFormatSupport(const VkFormat fmt,ImageTiling tiling=ImageTiling::Optimal)const{return CheckFormatSupport(fmt,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,tiling);}

    bool CheckColorAttachmentFormatSupport(const VkFormat fmt)const{return CheckFormatSupport(fmt,VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT,ImageTiling::Optimal);}
    bool CheckDepthStencilAttachFormatSupport(const VkFormat fmt)const{return CheckFormatSupport(fmt,VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT,ImageTiling::Optimal);}

public: //Create/Chagne

    Texture2D *CreateTexture2D(TextureData *);
    Texture2D *CreateTexture2D(TextureCreateInfo *ci);

    Texture2DArray *CreateTexture2DArray(TextureData *);
    Texture2DArray *CreateTexture2DArray(TextureCreateInfo *ci);
    Texture2DArray *CreateTexture2DArray(const uint32_t w,const uint32_t h,const uint32 l,const VkFormat fmt,const bool mipmaps);

    TextureCube *CreateTextureCube(TextureData *);
    TextureCube *CreateTextureCube(TextureCreateInfo *ci);

    void Clear(TextureCreateInfo *);

    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,                         const ValueArray<Image2DRegion> &,   VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,                         const RectScope2ui &,               VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,const void *data,const VkDeviceSize size,  const RectScope2ui &,               VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

//    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,             const ValueArray<Image2DRegion> &,  const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,                       const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2DArray(Texture2DArray *,const void *data,const VkDeviceSize size,const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

public:

    void Release(Texture *);
    void Destory(Texture *tex)
    {
        if(!tex)return;

        Release(tex);
        delete tex;
    }

public: // Load

    Texture2D *         LoadTexture2D(const OSString &,bool auto_mipmaps=false);
    TextureCube *       LoadTextureCube(const OSString &,bool auto_mipmaps=false);

    Texture2DArray *    CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,bool auto_mipmaps=false);
    bool                LoadTexture2DArray(Texture2DArray *,const uint32_t layer,const OSString &);

public: //TileData

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集

};//class TextureManager
VK_NAMESPACE_END
