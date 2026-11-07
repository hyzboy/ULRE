#include<hgl/graph/module/TextureLoadCommitter.h>
#include<hgl/graph/texture/TextureMemoryLoader.h>
#include<hgl/log/Log.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    uint64 TextureLoadCommitter::CommitTexture2D(const LoadedFile *file,
                                                bool auto_mipmaps,
                                                UploadPriority priority,
                                                uint32 bindless_handle,
                                                void (*callback)(TextureUploadTask *,void *),
                                                void *user_data)
    {
        if(!tex_manager||!file)
            return(0);

        const uint8 *data=file->GetData();
        const uint64 size=file->GetSize();

        if(!data||size==0)
        {
            GLogError(u8"[TextureLoadCommitter] empty payload: %s",to_u8(file->GetFilename().c_str(),file->GetFilename().Length()).c_str());
            return(0);
        }

        // VkTextureLoader 已在 Load 内把像素同步拷进 staging 缓冲，这里返回后字节即可释放
        const uint64 task_id=CreateTexture2DFromMemoryAsync(tex_manager,
                                                            data,
                                                            (int64)size,
                                                            auto_mipmaps,
                                                            priority,
                                                            bindless_handle,
                                                            callback,
                                                            user_data,
                                                            file->GetFilename());

        if(task_id==0)
            GLogError(u8"[TextureLoadCommitter] submit failed: %s",to_u8(file->GetFilename().c_str(),file->GetFilename().Length()).c_str());

        return(task_id);
    }

    bool TextureLoadCommitter::CommitTexture2DArrayLayer(const LoadedFile *file,
                                                         Texture2DArray *array,
                                                         uint32 layer)
    {
        if(!tex_manager||!file||!array)
            return(false);

        const uint8 *data=file->GetData();
        const uint64 size=file->GetSize();

        if(!data||size==0)
        {
            GLogError(u8"[TextureLoadCommitter] empty payload for array layer %u: %s",
                      layer,to_u8(file->GetFilename().c_str(),file->GetFilename().Length()).c_str());
            return(false);
        }

        return(LoadTexture2DLayerFromMemory(tex_manager,array,layer,data,(int64)size,file->GetFilename()));
    }

    uint64 TextureLoadCommitter::CommitTexture2DDirect(const LoadedFile *file,
                                                       DeviceBuffer *staging,
                                                       bool auto_mipmaps,
                                                       UploadPriority priority,
                                                       uint32 bindless_handle,
                                                       void (*callback)(TextureUploadTask *,void *),
                                                       void *user_data)
    {
        if(!tex_manager||!file)
        {
            if(staging)
                staging->Unmap();       // 口径统一:返回 0 前映射必定已关闭，调用方只管 delete
            return(0);
        }

        if(!staging)
            return(0);

        if(!file->IsDirectBuffer())
        {
            GLogError(u8"[TextureLoadCommitter] not a direct-buffer payload: %s",
                      to_u8(file->GetFilename().c_str(),file->GetFilename().Length()).c_str());
            staging->Unmap();
            return(0);
        }

        const void *data=file->GetData();
        const uint64 size=file->GetSize();

        if(!data||size==0)
        {
            GLogError(u8"[TextureLoadCommitter] empty direct payload: %s",
                      to_u8(file->GetFilename().c_str(),file->GetFilename().Length()).c_str());
            staging->Unmap();
            return(0);
        }

        // 零拷贝:字节就在 staging 里，这里只解析头部并转交上传任务(内部完成 Unmap)
        const uint64 task_id=CreateTexture2DFromStagingAsync(tex_manager,
                                                             data,
                                                             (int64)size,
                                                             staging,
                                                             auto_mipmaps,
                                                             priority,
                                                             bindless_handle,
                                                             callback,
                                                             user_data,
                                                             file->GetFilename());

        if(task_id==0)
            GLogError(u8"[TextureLoadCommitter] direct submit failed: %s",
                      to_u8(file->GetFilename().c_str(),file->GetFilename().Length()).c_str());

        return(task_id);
    }
}//namespace hgl::graph