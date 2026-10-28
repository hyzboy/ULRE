#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/type/BidirectionalMap.h>
#include<hgl/type/IDName.h>
#include<hgl/type/RectScope.h>
#include<hgl/graph/data/ImageRegion.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/graph/asset/TextureAsset.h>
#include<hgl/graph/module/TextureUploadTask.h>
#include<hgl/graph/module/TextureUploadQueue.h>
#include<hgl/type/ValueArray.h>

namespace hgl::graph{

class BindlessTextureManager;

GRAPH_MODULE_CLASS(TextureManager)
{
    DeviceQueue *texture_queue=nullptr;
    TextureCmdBuffer *texture_cmd_buf=nullptr;
    TextureUploadQueue *upload_queue=nullptr;

private:

    TextureID texture_serial=0;

    const TextureID AcquireID(){return texture_serial++;}                       ///<取得一个新的纹理ID

private:

    OrderedSet<VkImage> image_set;
    OrderedSet<Texture *> texture_set;                                           ///<纹理合集

    BidirectionalMap<TextureID,Texture *> texture_by_id;          ///< ID <-> Texture* 双向映射
    BidirectionalMap<OSString,Texture *> texture_by_filename;     ///< Filename <-> Texture* 双向映射

    /// 纹理句柄表（材质侧唯一入口）。**按文件名去重**：同一个文件只创建一个句柄，
    /// 多个材质共享同一份加载状态与同一张纹理。线性扫描即可 —— 只在材质装配期写入。
    ValueArray<TextureAsset *> texture_assets;

    const TextureID Add(Texture *);
    const TextureID Add(Texture *,const OSString &);

public:

    TextureManager(GraphicsContext *gc);
    virtual ~TextureManager();

    const VkFormatProperties GetFormatProperties(const VkFormat)const;

public:     //Buffer

    DeviceBuffer *CreateTransferSourceBuffer(const VkDeviceSize,const void *data_ptr=nullptr);

protected:

    void OnGraphicsContextChanged(GraphicsContext *gc) override;

private:

    void EnsureTransferResources();

private:     //Image

    VkImage CreateImage         (VkImageCreateInfo *);
    void    DestroyImage        (VkImage);

private:    //texture

    bool CopyBufferToImage      (const CopyBufferToImageInfo *info,VkPipelineStageFlags2 destinationStage);

    bool CopyBufferToImage      (Texture *,VkBuffer buf,const VkBufferImageCopy *,const int count,const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags2);//=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    bool CopyBufferToImage2D    (Texture *tex,VkBuffer buf,const VkBufferImageCopy *bic_list,const int bic_count,  VkPipelineStageFlags2 dstStage){return CopyBufferToImage(tex,buf,bic_list,   bic_count,  0,1,dstStage);}
    bool CopyBufferToImage2D    (Texture *tex,VkBuffer buf,const VkBufferImageCopy *bic,                           VkPipelineStageFlags2 dstStage){return CopyBufferToImage(tex,buf,bic,        1,          0,1,dstStage);}

    bool CopyBufferToImageCube  (Texture *tex,VkBuffer buf,const VkBufferImageCopy *bic_list,const int bic_count,  VkPipelineStageFlags2 dstStage){return CopyBufferToImage(tex,buf,bic_list,   bic_count,  0,6,dstStage);}
    bool CopyBufferToImageCube  (Texture *tex,VkBuffer buf,const VkBufferImageCopy *bic,                           VkPipelineStageFlags2 dstStage){return CopyBufferToImage(tex,buf,bic,        1,          0,6,dstStage);}

    bool CommitTexture2D        (Texture2D *,VkBuffer buf,VkPipelineStageFlags2 stage);
    bool CommitTexture2DMipmaps (Texture2D *,VkBuffer buf,const VkExtent3D &,uint32_t);

    bool CommitTextureCube          (TextureCube *,VkBuffer buf,const uint32_t mipmaps_zero_bytes,VkPipelineStageFlags2 stage);
    bool CommitTextureCubeMipmaps   (TextureCube *,VkBuffer buf,const VkExtent3D &,uint32_t);

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
    Texture2DArray *CreateTexture2DArray(const uint32_t w,const uint32_t h,const uint32 l,const VkFormat fmt,const uint32_t mip_levels=1);

    TextureCube *CreateTextureCube(TextureData *);
    TextureCube *CreateTextureCube(TextureCreateInfo *ci);

    void Clear(TextureCreateInfo *);

    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,                         const ValueArray<Image2DRegion> &,   VkPipelineStageFlags2=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,DeviceBuffer *buf,                         const RectScope2ui &,               VkPipelineStageFlags2=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    bool ChangeTexture2D(Texture2D *,const void *data,const VkDeviceSize size,  const RectScope2ui &,               VkPipelineStageFlags2=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    bool ChangeTexture2DArray(Texture2DArray *,DeviceBuffer *buf,                       const RectScope2ui &,         const uint32_t base_layer,const uint32_t layer_count,VkPipelineStageFlags2=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    /// 整条 mip 链逐层拷入（源 buffer 布局 = .Tex2D 文件内的 level0..level(n-1) 连续排列）
    bool ChangeTexture2DArrayMipmaps(Texture2DArray *,DeviceBuffer *buf,const VkExtent3D &extent,const uint32_t top_mipmap_bytes,const uint32_t base_layer,const uint32_t layer_count=1,VkPipelineStageFlags2=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

public:

    void Release(Texture *);
    void Destory(Texture *tex)
    {
        if(!tex)return;

        Release(tex);
        delete tex;
    }

    void Release() override;

public: // Load

    Texture2D *         LoadTexture2D(const OSString &,bool auto_mipmaps=false);
    uint64_t            LoadTexture2DAsync(const OSString &filename,
                                           bool auto_mipmaps = false,
                                           UploadPriority priority = UploadPriority::Normal,
                                           uint32_t bindless_handle = 0,
                                           void (*callback)(TextureUploadTask *, void *) = nullptr,
                                           void *user_data = nullptr);
    TextureCube *       LoadTextureCube(const OSString &,bool auto_mipmaps=false);

    Texture2DArray *    CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,const uint32_t mip_levels=1);
    bool                LoadTexture2DArray(Texture2DArray *,const uint32_t layer,const OSString &);
    bool                GenerateTexture2DArrayMipmaps(Texture2DArray *,const uint32_t layer);

public: // Async Upload Queue

    TextureUploadQueue *GetUploadQueue() { return upload_queue; }

    uint64_t CreateTexture2DAsync(TextureCreateInfo *tci,
                                 UploadPriority priority = UploadPriority::Normal,
                                 uint32_t bindless_handle = 0,
                                 void (*callback)(TextureUploadTask *, void *) = nullptr,
                                 void *user_data = nullptr);

    bool CancelUpload(uint64_t task_id);
    UploadTaskState GetUploadState(uint64_t task_id);
    void WaitUpload(uint64_t task_id);

    void UpdateUploadQueue(BindlessTextureManager *bindless_mgr = nullptr);

public: // TileData

    TileData *CreateTileData(const VkFormat video_format,const uint width,const uint height,const uint count);          ///<创建一个Tile数据集

public: // TextureAsset

    /**
     * 取（无则建）文件名对应的纹理句柄。
     *
     * 同一个文件（按 `MakeLoadedFileKey` 规范化后比较）只创建一个句柄 ⇒ 多个材质
     * 共享同一份「未加载 / 加载中 / 就绪」状态，也只会有一次读盘与一次上传。
     * 返回的句柄**初始为 `MetaOnly`**（只有文件名），由 `TextureStreamingSystem`
     * 驱动异步加载；也可以直接调用 `LoadPayload()` 走同步路径。
     *
     * 句柄归本表所有，调用方只持有裸观察指针（不要 delete）。
     */
    TextureAsset *CreateTextureAsset(const OSString &filename,bool auto_mipmaps=false);

    /// 把一张已就绪的纹理包装成句柄（不入文件名去重表：按一次性句柄处理，仍归本表所有）
    TextureAsset *WrapTextureAsset(Texture *texture);

    /// 句柄表只读视图（流式系统逐帧扫描 `MetaOnly` 句柄用）
    const ValueArray<TextureAsset *> &GetTextureAssets()const{return texture_assets;}

};//class TextureManager
}//namespace hgl::graph
