#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKImageCreateInfo.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/graph/module/RenderTargetManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/RenderTargetAsset.h>
#include<hgl/asset/LoadedFile.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
const VkFormatProperties TextureManager::GetFormatProperties(const VkFormat format) const
{
    return GetPhyDevice()->GetFormatProperties(format);
}

GRAPH_MODULE_CONSTRUCT(TextureManager)
{
    EnsureTransferResources();
    if (GetDevice())
        upload_queue = new GpuUploadQueue(GetDevice());
}

TextureManager::~TextureManager()
{
    SAFE_CLEAR(upload_queue);
    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);
}

void TextureManager::Release()
{
    for(int i=0;i<pending_texture_deletes_.GetCount();++i)
        if(pending_texture_deletes_[i].texture)
            delete pending_texture_deletes_[i].texture;
    pending_texture_deletes_.Clear();
    // 纹理句柄表归本管理器所有：先按稳定快照逐个 Release + delete，再动纹理本体，
    // 这样句柄的 OnUnloadPayload 一定跑在纹理对象仍然有效的时候。
    texture_asset_table.ReleaseAll();

    SAFE_CLEAR(upload_queue);

    // Delete using a stable snapshot so destructor-side unregister is safe.
    if (texture_set.GetCount() > 0)
    {
        ValueArray<Texture *> to_delete;
        for (auto *tex : texture_set)
            to_delete.Add(tex);

        for (int i = 0; i < to_delete.GetCount(); ++i)
            delete to_delete[i];

        texture_set.Clear();
    }

    if (image_set.GetCount() > 0)
        image_set.Clear();

    if (texture_by_id.GetCount() > 0)
        texture_by_id.Clear();

    for (auto &cache : texture_by_filename)
        cache.Clear();
    for (auto &cache : texture_cube_by_filename)
        cache.Clear();

    // Clean up texture transfer resources (queue holds fences that need cleanup)
    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);
}

void TextureManager::OnGraphicsContextChanged(GraphicsContext *)
{
    EnsureTransferResources();
}

void TextureManager::EnsureTransferResources()
{

    auto dev=GetDevice();
    auto phy_device=GetPhyDevice();

    if(!dev || !phy_device)
        return;

    if(!upload_queue)
        upload_queue=new GpuUploadQueue(dev);

    if(texture_cmd_buf && texture_queue)
        return;

    if(!texture_cmd_buf)
        texture_cmd_buf=dev->CreateTextureCommandBuffer("TextureManager");

    if(!texture_queue)
        texture_queue=dev->CreateQueue("TextureManager");

    if(texture_cmd_buf && texture_queue)
        texture_serial=0;
}

void TextureManager::OnTextureDestroying(Texture *tex)
{
    if(!tex)
        return;

    if(auto *gc=GetGraphicsContext())
    {
        if(auto *bindless=gc->GetBindlessTextureManager())
            bindless->NotifyTextureDestroyed(tex->GetUniqueID());
    }

    Release(tex);
}

UploadLaneSignal TextureManager::GetUploadLaneSignal() const
{
    UploadLaneSignal lane;

    if(!upload_queue)
        return lane;

    lane.semaphore = upload_queue->GetUploadLane();
    lane.value     = upload_queue->GetUploadLaneValue();
    return lane;
}

const TextureID TextureManager::Add(Texture *tex)
{
    if(!tex)
        return(-1);

    if(tex->GetOwner()!=this)
        return(-2);

    if(texture_set.Contains(tex))
        return tex->GetID();

    texture_set.Add(tex);
    texture_by_id.Add(tex->GetID(),tex);

    VulkanDevice *dev = GetDevice();
    if (dev)
    {
        AnsiString name = "Texture_" + AnsiString::numberOf(static_cast<int>(tex->GetID()));
        dev->TrackTexture(tex, name);
    }

    return tex->GetID();
}

const TextureID TextureManager::Add(Texture *tex,const OSString &tn,bool auto_mipmaps)
{
    TextureID id=Add(tex);

    if(id<0)
        return id;

    if(!tn.IsEmpty())
        texture_by_filename[auto_mipmaps ? 1 : 0].Add(tn,tex);

    return id;
}

void TextureManager::Release(Texture *tex)
{
    if(!tex)
        return;

    if(!texture_set.Contains(tex))
        return;

    texture_set.Delete(tex);
    texture_by_id.RemoveByValue(tex);          ///<从ID映射删除
    for (auto &cache : texture_by_filename)
        cache.RemoveByValue(tex);
    for (auto &cache : texture_cube_by_filename)
        cache.RemoveByValue(tex);
}

void TextureManager::Destory(Texture *tex)
{
    if(!tex)
        return;

    if(auto *gc=GetGraphicsContext())
    {
        if(auto *bindless=gc->GetBindlessTextureManager())
            bindless->UnregisterTexture(tex->GetUniqueID(),texture_frame_serial_);
    }

    PendingTextureDelete pending;
    pending.texture=tex;
    pending.retire_frame=texture_frame_serial_+HGL_FRAME_SLOT_TOTAL;
    pending_texture_deletes_.Add(pending);
    Release(tex);    ///<从文件名映射删除
}

Texture *TextureManager::FindTextureByFilename(const OSString &filename,bool auto_mipmaps)const
{
    if(filename.IsEmpty())
        return(nullptr);

    Texture *tex=nullptr;

    if(!texture_by_filename[auto_mipmaps ? 1 : 0].Get(filename,tex))
        return(nullptr);

    return tex;
}

bool TextureManager::RegisterTextureFilename(const OSString &filename,Texture *texture,bool auto_mipmaps)
{
    if(filename.IsEmpty()||!texture)
        return(false);

    if(!dynamic_cast<Texture2D *>(texture))
    {
        GLogError(u8"[TextureManager] Cannot register a non-2D texture in the 2D filename cache");
        return(false);
    }

    // 只登记本管理器名下的纹理：否则 Release(tex) 的 RemoveByValue 清不掉这条映射，
    // 会留下指向已销毁纹理的悬垂条目。
    if(!texture_set.Contains(texture))
        return(false);

    Texture *existing=nullptr;

    auto &cache=texture_by_filename[auto_mipmaps ? 1 : 0];
    if(cache.Get(filename,existing))
        return(existing==texture);

    cache.Add(filename,texture);

    return(true);
}

Texture2D *CreateTexture2DFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps);
uint64_t CreateTexture2DFromFileAsync(TextureManager *tm,
                                     const OSString &filename,
                                     bool auto_mipmaps,
                                     UploadPriority priority,
                                     uint32_t bindless_handle,
                                     void (*callback)(GpuUploadTask *, void *),
                                     void *user_data);

Texture2D *TextureManager::LoadTexture2D(const OSString &filename,bool auto_mipmaps)
{
    // 无扩展名写法先探测补全（`x` → `x.Tex2D` 等），补全后的名字同时作缓存键，
    // 让 `tex2d:x`、`file:x`、`x.Tex2D` 收敛到同一张纹理。
    const OSString probed=ProbeTextureURI(filename);

    Texture *cached=nullptr;
    if(texture_by_filename[auto_mipmaps ? 1 : 0].Get(probed,cached))
        return static_cast<Texture2D *>(cached);

    Texture2D *tex=CreateTexture2DFromFile(this,probed,auto_mipmaps);

    if(tex)
    {
        Add(tex,probed,auto_mipmaps);

    //#ifdef _DEBUG
    //    DebugUtils *du=device->GetDebugUtils();

    //    if(du)
    //    {
    //        const U8String name=U8_TEXT("Tex2D:")+ToUTF8String(filename);
    //
    //        du->SetImage(tex->GetImage(),(char *)(name.c_str()));
    //    }
    //#endif//_DEBUG
    }

    return tex;
}

uint64_t TextureManager::LoadTexture2DAsync(const OSString &filename,
                                            bool auto_mipmaps,
                                            UploadPriority priority,
                                            uint32_t bindless_handle,
                                            void (*callback)(GpuUploadTask *, void *),
                                            void *user_data)
{
    return CreateTexture2DFromFileAsync(this, ProbeTextureURI(filename), auto_mipmaps, priority, bindless_handle, callback, user_data);
}

Texture2DArray *TextureManager::CreateTexture2DArray(const AnsiString &name,const uint32_t width,const uint32_t height,const uint32_t layer,const VkFormat &fmt,const uint32_t mip_levels)
{
    Texture2DArray *ta=CreateTexture2DArray(width,height,layer,fmt,mip_levels);

    if(ta)
        Add(ta);
    else
        return nullptr;

    GLogInfo("[Texture2DArray] create name=%s %ux%u layers=%u fmt=%s mip_levels=%u",
             name.c_str(),
             width,height,layer,
             GetVulkanFormatName(fmt)?GetVulkanFormatName(fmt):"unknown",
             ta->GetMipLevel());

    //#ifdef _DEBUG
    //    DebugUtils *du=device->GetDebugUtils();
    //
    //    if(du)
    //    {
    //        du->SetImage(ta->GetImage(),"Tex2DArrayImage:"+name);
    //        du->SetImageView(ta->GetVulkanImageView(),"Tex2DArrayImageView:"+name);
    //        du->SetDeviceMemory(ta->GetDeviceMemory(),"Tex2DArrayMemory:"+name);
    //    }
    //#endif//_DEBUG

    return ta;
}

bool LoadTexture2DLayerFromFile(TextureManager *tm,Texture2DArray *t2d,const uint32_t layer,const OSString &filename);

bool TextureManager::LoadTexture2DArray(Texture2DArray *ta,const uint32_t layer,const OSString &filename)
{
    if(!ta)return(false);

    if(!LoadTexture2DLayerFromFile(this,ta,layer,filename))
        return(false);

    return(true);
}

    namespace
    {
        /**
         * 层 URI 允许写成 `file:<path>`，也允许直接写裸路径；真正打开文件前统一取解析后的
         * `path`。（`rt:` 与嵌套数组在规约解析阶段就已经被拒绝了，这里不会再出现。）
         */
        OSString ResolveTextureArrayLayerFilename(const OSString &uri)
        {
            const TextureURI parsed=TextureURI::Parse(uri);

            if(parsed.IsValid()&&!parsed.path.IsEmpty())
                return parsed.path;

            return uri;
        }
    }//namespace

Texture2DArray *TextureManager::BuildTextureArray(const TextureArraySpec &spec)
{
    const int layer_count=spec.layers.GetCount();

    if(layer_count<=0)
    {
        GLogError("[TextureManager] BuildTextureArray 规约为空：%s",spec.name.c_str());
        return(nullptr);
    }

    // 尺寸与格式只能从**第 0 层**读出来（规约里故意不写，避免第二份真源）。
    // 第 0 层加载后留在 texture_by_filename 里复用：删掉它会让缓存指向已释放对象。
    Texture2D *probe=LoadTexture2D(ResolveTextureArrayLayerFilename(spec.layers[0]->uri),true);

    if(!probe)
    {
        GLogError("[TextureManager] BuildTextureArray 首层加载失败：%s (%ls)",
                  spec.name.c_str(),spec.layers[0]->uri.c_str());
        return(nullptr);
    }

    const uint32_t    width =probe->GetWidth();
    const uint32_t    height=probe->GetHeight();
    const VkFormat    format=probe->GetFormat();
    const uint32_t    mips  =spec.mipmaps>0
                            ? spec.mipmaps
                            : probe->GetMipLevel();

    Texture2DArray *array=CreateTexture2DArray(spec.name,
                                               width,height,
                                               static_cast<uint32_t>(layer_count),
                                               format,mips);

    if(!array)
    {
        GLogError("[TextureManager] BuildTextureArray 建数组失败：%s %ux%u layers=%d",
                  spec.name.c_str(),width,height,layer_count);
        return(nullptr);
    }

    for(int i=0;i<layer_count;i++)
    {
        if(LoadTexture2DArray(array,static_cast<uint32_t>(i),
                             ResolveTextureArrayLayerFilename(spec.layers[i]->uri)))
            continue;

        // 层失败没有「部分可用」的说法：尺寸/格式已经定死，缺层的数组采样结果不可信。
        GLogError("[TextureManager] BuildTextureArray 第 %d/%d 层加载失败：%s (%ls)",
                  i,layer_count,spec.name.c_str(),spec.layers[i]->uri.c_str());
        return(nullptr);
    }

    GLogInfo("[TextureManager] 纹理数组装配完成：%s %ux%u layers=%d mip_levels=%u",
             spec.name.c_str(),width,height,layer_count,array->GetMipLevel());

    return(array);
}

TextureCube *CreateTextureCubeFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps);

TextureCube *TextureManager::LoadTextureCube(const OSString &filename,bool auto_mipmaps)
{
    // 裸路径无扩展名时按立方体候选探测（`.TexCube` 优先）；带 `cubefile:/texcube:`
    // 前缀的由 scheme 定形态。
    const OSString probed=ProbeTextureURI(filename,TextureURIKind::CubeFile);

    Texture *cached=nullptr;
    if(texture_cube_by_filename[auto_mipmaps ? 1 : 0].Get(probed,cached))
        return static_cast<TextureCube *>(cached);

    TextureCube *tex=CreateTextureCubeFromFile(this,probed,auto_mipmaps);

    if(tex)
    {
        Add(tex);
        texture_cube_by_filename[auto_mipmaps ? 1 : 0].Add(probed,tex);
    }

    return tex;
}

uint64_t TextureManager::CreateTexture2DAsync(TextureCreateInfo *tci,
                                             UploadPriority priority,
                                             uint32_t bindless_handle,
                                             void (*callback)(GpuUploadTask *, void *),
                                             void *user_data)
{
    if (!tci)
        return 0;

    if (!upload_queue)
    {
        Clear(tci);
        return 0;
    }

    if (tci->extent.width * tci->extent.height <= 0)
    {
        Clear(tci);
        return 0;
    }

    if (tci->target_mipmaps == 0)
        tci->target_mipmaps = (tci->origin_mipmaps > 1 ? tci->origin_mipmaps : 1);

    if (!tci->image)
    {
        Image2DCreateInfo ici(tci->usage, tci->tiling, tci->format, tci->extent, tci->target_mipmaps);

        uint32_t queue_families[2] = {
            GetDevice()->GetGraphicsFamilyIndex(),
            GetDevice()->GetTransferFamilyIndex()
        };
        if (queue_families[0] != queue_families[1] && queue_families[1] != VK_QUEUE_FAMILY_IGNORED)
        {
            ici.sharingMode = VK_SHARING_MODE_CONCURRENT;
            ici.queueFamilyIndexCount = 2;
            ici.pQueueFamilyIndices = queue_families;
        }

        tci->image = CreateImage(&ici);
        if (!tci->image)
        {
            Clear(tci);
            return 0;
        }

        tci->memory = GetDevice()->CreateMemory(tci->image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            ObjectNameBuilder(tci->name.IsEmpty() ? "AsyncTexture2DMemory" : (const char *)tci->name.c_str()));
    }

    if (!tci->image_view)
        tci->image_view = CreateImageView2D(GetVkDevice(), tci->format, tci->extent, tci->target_mipmaps, tci->aspect, tci->image);

    Texture2D *tex = CreateTexture2D(new TextureData(tci));
    if (!tex)
    {
        Clear(tci);
        return 0;
    }

    GpuUploadTask *task = new GpuUploadTask();
    task->priority = priority;
    task->target_texture = tex;
    task->tci = tci;
    task->bindless_handle = bindless_handle;
    task->auto_mipmaps = (tci->target_mipmaps > 1 && tci->origin_mipmaps <= 1);
    task->staging_bytes = tci->total_bytes > 0 ? tci->total_bytes : (tci->buffer ? tci->buffer->GetSize() : 0);
    task->on_complete = callback;
    task->user_data = user_data;

    return upload_queue->Enqueue(task);
}

bool TextureManager::CancelUpload(uint64_t task_id)
{
    return upload_queue ? upload_queue->CancelUpload(task_id) : false;
}

UploadTaskState TextureManager::GetUploadState(uint64_t task_id)
{
    return upload_queue ? upload_queue->GetTaskState(task_id) : UploadTaskState::Cancelled;
}

void TextureManager::WaitUpload(uint64_t task_id)
{
    if (upload_queue)
        upload_queue->WaitTask(task_id);
}

void TextureManager::UpdateUploadQueue(BindlessTextureManager *bindless_mgr)
{
    ++texture_frame_serial_;
    if(bindless_mgr)
        bindless_mgr->ReclaimRetiredSlots(texture_frame_serial_);

    ValueArray<PendingTextureDelete> remain;
    for(int i=0;i<pending_texture_deletes_.GetCount();++i)
    {
        PendingTextureDelete pending=pending_texture_deletes_[i];
        if(pending.retire_frame<=texture_frame_serial_)
        {
            if(pending.texture)
                delete pending.texture;
        }
        else
        {
            remain.Add(pending);
        }
    }
    pending_texture_deletes_=remain;

    if (!upload_queue)
        return;

    upload_queue->Update();

    ValueArray<GpuUploadTask *> completed;
    upload_queue->ExtractCompletedTasks(completed);

    for (int i = 0; i < completed.GetCount(); ++i)
    {
        GpuUploadTask *task = completed[i];
        if (task->state == UploadTaskState::Completed)
        {
            if (bindless_mgr && task->bindless_handle > 0 && task->target_texture)
            {
                bindless_mgr->UpdateTextureHandle(task->bindless_handle, task->target_texture);
            }
        }
        else if (task->state == UploadTaskState::Discarded)
        {
            if (task->target_texture)
            {
                Destory(task->target_texture);
                task->target_texture = nullptr;
            }
        }
        delete task;
    }
}

// ── TextureAsset：材质侧统一纹理句柄 ──────────────────────────────────────────

TextureAsset *TextureManager::CreateTextureAsset(const OSString &filename,bool auto_mipmaps)
{
    return texture_asset_table.CreateFromFilename(filename,auto_mipmaps);
}

TextureAsset *TextureManager::GetOrCreateTextureArrayAsset(const TextureURI &uri)
{
    return texture_asset_table.GetOrCreateTextureArray(uri);
}

TextureAsset *TextureManager::FindRenderTargetAsset(const OSString &target,const OSString &attachment)const
{
    return texture_asset_table.FindRenderTarget(target,attachment);
}

TextureAsset *TextureManager::GetOrCreateRenderTargetAsset(const OSString &target,const OSString &attachment)
{
    return texture_asset_table.GetOrCreateRenderTarget(target,attachment);
}

int TextureManager::BindRenderTargetTextures(const OSString &target,Texture *texture,const OSString &attachment)
{
    return texture_asset_table.BindRenderTarget(target,texture,attachment);
}

void TextureManager::UnbindRenderTargetTextures(const OSString &target)
{
    texture_asset_table.UnbindRenderTarget(target);
}

int TextureManager::MarkRenderTargetContentReady(const OSString &target)
{
    return texture_asset_table.MarkRenderTargetContentReady(target);
}

TextureAsset *TextureManager::WrapTextureAsset(Texture *texture)
{
    return texture_asset_table.WrapTexture(texture);
}

}//namespace hgl::graph