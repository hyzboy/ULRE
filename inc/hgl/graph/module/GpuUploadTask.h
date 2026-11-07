#pragma once

#include <hgl/vk/VK.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <hgl/sync/Atomic.h>
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

    /**
     * GPU 上传任务的物理传输形态。
     * 所有发往显存的 DMA 传输在硬件上只有两种：
     *  - BufferToImage：vkCmdCopyBufferToImage（纹理 / Cubemap / 纹理数组）
     *  - BufferToBuffer：vkCmdCopyBuffer（VBO/IBO、SSBO 等任意原始字节块）
     */
    enum class GpuUploadKind : uint8_t
    {
        BufferToImage,      // Staging Buffer -> VkImage
        BufferToBuffer      // Staging Buffer -> Device-Local Buffer
    };

    /** BufferToBuffer 单段拷贝区域（源偏移为 Staging 缓冲内绝对偏移） */
    struct BufferCopyRegion
    {
        VkDeviceSize src_offset = 0;   // Staging 源缓冲内起始偏移
        VkDeviceSize dst_offset = 0;   // 目标缓冲内起始偏移
        VkDeviceSize size       = 0;   // 拷贝字节数

        bool operator==(const BufferCopyRegion &o) const
        {
            return src_offset == o.src_offset && dst_offset == o.dst_offset && size == o.size;
        }
    };

    class Fence;

    /**
     * 通用 GPU 异步上传任务描述。
     * 由原 TextureUploadTask 泛化而来：纹理支路（BufferToImage）字段与语义保持不变，
     * 新增缓冲支路（BufferToBuffer）承载几何 VBO/IBO、SSBO 等零拷贝直传任务。
     */
    struct GpuUploadTask
    {
        GpuUploadKind       kind             = GpuUploadKind::BufferToImage;

        uint64_t            task_id          = 0;
        UploadPriority      priority         = UploadPriority::Normal;
        UploadBackendType   backend          = UploadBackendType::Auto;
        UploadTaskState     state            = UploadTaskState::Queued;

        DeviceBuffer *      staging_buffer   = nullptr;      // DMA 路径的暂存缓冲（非环形时由任务持有）
        VkDeviceSize        staging_bytes    = 0;
        bool                is_ring_staging  = false;        // 是否使用共享环形 Staging 缓冲
        bool                owns_staging     = true;         // 完成时是否由任务销毁 staging_buffer（共享 Staging 置 false）
        VkDeviceSize        ring_offset      = 0;            // 环形/直通缓冲内的起始偏移

        Fence *             fence            = nullptr;      // 同步栅栏

        // ── BufferToImage 支路（纹理）──
        Texture *           target_texture   = nullptr;
        TextureCreateInfo * tci              = nullptr;      // 纹理创建参数与原始像素

        uint32_t            bindless_handle  = 0;            // 分配的 Bindless 槽位

        bool                auto_mipmaps     = false;        // 是否需要 Blit 生成 Mipmap

        // ── BufferToBuffer 支路（几何/SSBO/原始字节）──
        DeviceBuffer *      target_buffer    = nullptr;      // Device-Local 目标缓冲（由资产方持有，任务不销毁）
        VkDeviceSize        dst_offset       = 0;            // 单段拷贝：目标偏移（copy_regions 为空时生效）
        VkDeviceSize        copy_size        = 0;            // 单段拷贝：字节数（0 = 取全部 staging_bytes）
        ValueArray<BufferCopyRegion> copy_regions;           // 多段区域；src_offset 为 Staging 内绝对偏移

        void (*on_complete)(GpuUploadTask *, void *) = nullptr;
        void *              user_data        = nullptr;

        GpuUploadTask() = default;
        ~GpuUploadTask()
        {
            if (staging_buffer)
            {
                if (!is_ring_staging && owns_staging)
                    delete staging_buffer;
                staging_buffer = nullptr;
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

        /** 解析缓冲支路的最终拷贝区域列表（单段字段归一化为 1 个 region） */
        const ValueArray<BufferCopyRegion> &ResolveCopyRegions()
        {
            if (copy_regions.IsEmpty())
            {
                BufferCopyRegion r;
                r.src_offset = ring_offset;
                r.dst_offset = dst_offset;
                r.size = (copy_size > 0) ? copy_size : staging_bytes;
                copy_regions.Add(r);
            }
            return copy_regions;
        }
    };
}
