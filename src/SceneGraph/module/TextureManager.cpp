#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKImageCreateInfo.h>
#include<hgl/vk/VKImageView.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/graph/module/RenderPassManager.h>
#include<hgl/object/ObjectTracker.h>
#include<hgl/log/Log.h>

namespace hgl::graph{
const VkFormatProperties TextureManager::GetFormatProperties(const VkFormat format) const
{
    return GetPhyDevice()->GetFormatProperties(format);
}

GRAPH_MODULE_CONSTRUCT(TextureManager)
{
    HGL_CAPTURE_SCOPE();
    EnsureTransferResources();
    if (GetDevice())
        upload_queue = new TextureUploadQueue(GetDevice());
}

TextureManager::~TextureManager()
{
    SAFE_CLEAR(upload_queue);
    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);
}

void TextureManager::Release()
{
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

    if (texture_by_filename.GetCount() > 0)
        texture_by_filename.Clear();

    // Clean up texture transfer resources (queue holds fences that need cleanup)
    SAFE_CLEAR(texture_queue);
    SAFE_CLEAR(texture_cmd_buf);
}

void TextureManager::OnGraphicsContextChanged(GraphicsContext *)
{
    HGL_CAPTURE_SCOPE();
    EnsureTransferResources();
}

void TextureManager::EnsureTransferResources()
{
    HGL_CAPTURE_SCOPE();

    auto dev=GetDevice();
    auto phy_device=GetPhyDevice();

    if(!dev || !phy_device)
        return;

    if(!upload_queue)
        upload_queue=new TextureUploadQueue(dev);

    if(texture_cmd_buf && texture_queue)
        return;

    if(!texture_cmd_buf)
        texture_cmd_buf=dev->CreateTextureCommandBuffer("TextureManager");

    if(!texture_queue)
        texture_queue=dev->CreateQueue("TextureManager");

    if(texture_cmd_buf && texture_queue)
        texture_serial=0;
}

const TextureID TextureManager::Add(Texture *tex)
{
    if(!tex)
        return(-1);

    if(tex->GetManager()!=this)
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

const TextureID TextureManager::Add(Texture *tex,const OSString &tn)
{
    TextureID id=Add(tex);

    if(id<0)
        return id;

    if(!tn.IsEmpty())
        texture_by_filename.Add(tn,tex);

    return id;
}

void TextureManager::Release(Texture *tex)
{
    if(!tex)
        return;

    if(!texture_set.Contains(tex))
        return;

    texture_set.Delete(tex);
    texture_by_id.DeleteByValue(tex);          ///<从ID映射删除
    texture_by_filename.DeleteByValue(tex);    ///<从文件名映射删除
}

Texture2D *CreateTexture2DFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps);
uint64_t CreateTexture2DFromFileAsync(TextureManager *tm,
                                     const OSString &filename,
                                     bool auto_mipmaps,
                                     UploadPriority priority,
                                     uint32_t bindless_handle,
                                     void (*callback)(TextureUploadTask *, void *),
                                     void *user_data);

Texture2D *TextureManager::LoadTexture2D(const OSString &filename,bool auto_mipmaps)
{
    Texture2D *tex;

    if(texture_by_filename.Get(filename,(Texture *&)tex))
        return tex;

    tex=CreateTexture2DFromFile(this,filename,auto_mipmaps);

    if(tex)
    {
        Add(tex,filename);

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
                                            void (*callback)(TextureUploadTask *, void *),
                                            void *user_data)
{
    return CreateTexture2DFromFileAsync(this, filename, auto_mipmaps, priority, bindless_handle, callback, user_data);
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

TextureCube *CreateTextureCubeFromFile(TextureManager *tm,const OSString &filename,bool auto_mipmaps);

TextureCube *TextureManager::LoadTextureCube(const OSString &filename,bool auto_mipmaps)
{
    TextureCube *tex;

    if(texture_by_filename.Get(filename,(Texture *&)tex))
        return tex;

    tex=CreateTextureCubeFromFile(this,filename,auto_mipmaps);

    if(tex)
    {
        Add(tex,filename);
    }

    return tex;
}

uint64_t TextureManager::CreateTexture2DAsync(TextureCreateInfo *tci,
                                             UploadPriority priority,
                                             uint32_t bindless_handle,
                                             void (*callback)(TextureUploadTask *, void *),
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

        if (GetPhyDevice() && GetPhyDevice()->SupportHostImageCopyFormat(tci->format))
            ici.usage |= VK_IMAGE_USAGE_HOST_TRANSFER_BIT;

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

    TextureUploadTask *task = new TextureUploadTask();
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
    if (!upload_queue)
        return;

    upload_queue->Update();

    ValueArray<TextureUploadTask *> completed;
    upload_queue->ExtractCompletedTasks(completed);

    for (int i = 0; i < completed.GetCount(); ++i)
    {
        TextureUploadTask *task = completed[i];
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

}//namespace hgl::graph
