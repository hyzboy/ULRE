#pragma once

#include<hgl/graph/manager/GraphManager.h>
#include<hgl/type/SortedSets.h>
#include<hgl/type/IDName.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/ImageRegion.h>

VK_NAMESPACE_BEGIN

class TextureManager:public GraphManager
{
    SortedSets<Texture *> texture_list;

    DeviceQueue *texture_queue;
    TextureCmdBuffer *texture_cmd_buf;

public:

    TextureManager();
    virtual ~TextureManager();

public:     //Buffer

    DeviceBuffer *CreateTransferSourceBuffer(const VkDeviceSize,const void *data_ptr=nullptr);

public:     //Image

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

    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,                         const List<Image2DRegion> &,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,                         const RectScope2ui &,       VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,const void *data,const VkDeviceSize size,  const RectScope2ui &,       VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

//    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,             const List<Image2DRegion> &,  const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,                       const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2DArray(Texture2DArray *,const void *data,const VkDeviceSize size,const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

};//class TextureManager

VK_NAMESPACE_END
