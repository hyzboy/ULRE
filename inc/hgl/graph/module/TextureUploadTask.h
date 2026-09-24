#pragma once

#include <hgl/vk/VK.h>
#include <hgl/type/String.h>
#include <hgl/thread/Atomic.h>
#include <hgl/vk/VKTexture.h>
#include <hgl/vk/buffer/DeviceBuffer.h>
#include <hgl/vk/VKQueue.h>
#include <hgl/vk/VKSemaphore.h>
#include <hgl/vk/VKFence.h>

namespace hgl::graph
{
    class Texture;

    enum class UploadPriority : uint8_t
    {
        Immediate = 0,      // 立即/关键路径（主UI、近身必须材质，可同步等待或首选调度）
        High      = 1,      // 高优先级（可视视锥内主贴图、Mip0）
        Normal    = 2,      // 普通优先级（通用材质、次级贴图）
        Low       = 3,      // 低优先级（预加载、远景、环境贴图）
        Count     = 4
    };

    enum class UploadTaskState : uint8_t
    {
        Queued,             // 在队列等待调度
        InFlight,           // 已提交至 GPU 传输或正在直写
        Completed,          // 上传完成，已就绪
        Cancelled,          // 调度前已被取消
        Discarded           // GPU 执行中被撤销，完成后直接丢弃
    };

    enum class UploadBackendType : uint8_t
    {
        Auto,               // 自动判定
        GpuTransferDma,     // 专用 Transfer 队列 DMA 传输
        GpuGraphicsDma      // Graphics 队列 DMA 传输（用于需 Blit 生成 Mipmap 等）
    };

    class Semaphore;
    class Fence;

    struct TextureUploadTask
    {
        uint64_t            task_id          = 0;
        UploadPriority      priority         = UploadPriority::Normal;
        UploadBackendType   backend          = UploadBackendType::Auto;
        UploadTaskState     state            = UploadTaskState::Queued;

        Texture *           target_texture   = nullptr;
        TextureCreateInfo * tci              = nullptr;      // 纹理创建参数与原始像素

        DeviceBuffer *      staging_buffer   = nullptr;      // DMA 路径的暂存缓冲
        VkDeviceSize        staging_bytes    = 0;
        bool                is_ring_staging  = false;        // 是否使用共享环形 Staging 缓冲
        VkDeviceSize        ring_offset      = 0;            // 环形缓冲内的起始偏移

        Semaphore *         transfer_sem     = nullptr;      // Transfer 队列完成信号量包装
        VkSemaphore         signal_semaphore = VK_NULL_HANDLE; // Transfer 队列完成信号量句柄
        Fence *             fence            = nullptr;      // 同步栅栏

        uint32_t            bindless_handle  = 0;            // 分配的 Bindless 槽位

        bool                auto_mipmaps     = false;        // 是否需要 Blit 生成 Mipmap

        void (*on_complete)(TextureUploadTask *, void *) = nullptr;
        void *              user_data        = nullptr;

        TextureUploadTask() = default;
        ~TextureUploadTask()
        {
            if (staging_buffer)
            {
                if (!is_ring_staging)
                    delete staging_buffer;
                staging_buffer = nullptr;
            }
            if (transfer_sem)
            {
                delete transfer_sem;
                transfer_sem = nullptr;
            }
            if (fence)
            {
                delete fence;
                fence = nullptr;
            }
            if (tci)
            {
                if (tci->buffer)
                {
                    delete tci->buffer;
                    tci->buffer = nullptr;
                }
                delete tci;
                tci = nullptr;
            }
        }
    };
}
