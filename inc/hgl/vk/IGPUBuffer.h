#pragma once

#include<hgl/vk/VK.h>
#include<cstddef>
#include<string>
#include<vector>
#include<algorithm>

namespace hgl::graph{

/**
 * Layer2 统一接口 —— 外部开发者面向此接口，不直接接触底层实现
 *
 * 实现类（各自独立，互不依赖）：
 *   StagedBuffer  — staging(CPU) + device(GPU) 两块内存，Write/Unmap 自动置脏，
 *                   构造时注册到 VulkanDevice::gpu_buffer_registry，
 *                   ECS RenderBufferUploadSystem 每帧轮询，调用 CopyToDevice 完成上传。
 *   ReBarBuffer   — 单块 CPU-visible + Device-local 内存（需硬件 ReBAR 支持），
 *                   Write 直接可见，CopyToDevice 为 no-op，同样注册 registry。
 *   RingBuffer    — 多帧轮转的 CPU-visible buffer，不注册 registry，
 *                   由 TransformAssignmentBuffer 等外部逻辑每帧直接驱动，
 *                   不经过 RenderBufferUploadSystem。
 *
 * 规则：
 * - MarkDirty/IsDirty/ClearDirty 只记录状态，不触发任何提交
 * - CopyToDevice 由 ECS System (RenderBufferUploadSystem) 统一调用
 * - 实现类不持有任何队列引用
 */
class IGPUBuffer
{
public:
    struct DirtyRange
    {
        VkDeviceSize offset = 0;
        VkDeviceSize size   = 0;
    };

protected:
    const std::string buffer_name;  // e.g. "PlaneGrid:Position", set once at construction

    IGPUBuffer() = delete;
    explicit IGPUBuffer(const std::string &name) : buffer_name(name) {}

protected:
    bool tracked_dirty = false;
    VkDeviceSize tracked_dirty_offset = 0;
    VkDeviceSize tracked_dirty_size = 0;
    std::vector<DirtyRange> tracked_dirty_ranges;

    static bool NormalizeRange(const VkDeviceSize buffer_size, VkDeviceSize &offset, VkDeviceSize &size)
    {
        if (offset >= buffer_size)
            return false;

        if (size == VK_WHOLE_SIZE || offset + size > buffer_size)
            size = buffer_size - offset;

        return size > 0;
    }

    static void MergeRanges(std::vector<DirtyRange> &ranges)
    {
        if (ranges.size() <= 1)
            return;

        std::sort(ranges.begin(), ranges.end(), [](const DirtyRange &a, const DirtyRange &b)
        {
            return a.offset < b.offset;
        });

        size_t write_index = 0;
        for (size_t read_index = 1; read_index < ranges.size(); ++read_index)
        {
            auto &last = ranges[write_index];
            const auto &cur = ranges[read_index];

            const VkDeviceSize last_end = last.offset + last.size;
            const VkDeviceSize cur_end = cur.offset + cur.size;

            if (cur.offset <= last_end)
            {
                if (cur_end > last_end)
                    last.size = cur_end - last.offset;
            }
            else
            {
                ++write_index;
                ranges[write_index] = cur;
            }
        }

        ranges.resize(write_index + 1);
    }

    void TrackDirtyRange(const VkDeviceSize buffer_size, VkDeviceSize offset, VkDeviceSize size)
    {
        if (!NormalizeRange(buffer_size, offset, size))
            return;

        if (!tracked_dirty)
        {
            tracked_dirty = true;
            tracked_dirty_offset = offset;
            tracked_dirty_size = size;
        }
        else
        {
            const VkDeviceSize end1 = tracked_dirty_offset + tracked_dirty_size;
            const VkDeviceSize end2 = offset + size;

            tracked_dirty_offset = (std::min)(tracked_dirty_offset, offset);
            tracked_dirty_size = (std::max)(end1, end2) - tracked_dirty_offset;
        }

        tracked_dirty_ranges.push_back({offset, size});
        MergeRanges(tracked_dirty_ranges);
    }

    void TrackDirtyRanges(const VkDeviceSize buffer_size, const DirtyRange *ranges, const size_t count)
    {
        if (!ranges || count == 0)
            return;

        for (size_t i = 0; i < count; ++i)
            TrackDirtyRange(buffer_size, ranges[i].offset, ranges[i].size);
    }

    bool HasTrackedDirty() const { return tracked_dirty; }
    VkDeviceSize GetTrackedDirtyOffset() const { return tracked_dirty_offset; }
    VkDeviceSize GetTrackedDirtySize() const { return tracked_dirty_size; }
    const std::vector<DirtyRange>& GetTrackedDirtyRanges() const { return tracked_dirty_ranges; }

    void ClearTrackedDirty()
    {
        tracked_dirty = false;
        tracked_dirty_offset = 0;
        tracked_dirty_size = 0;
        tracked_dirty_ranges.clear();
    }

public:
    virtual ~IGPUBuffer() = default;

    const std::string &GetBufferName() const { return buffer_name; }

    // 写入数据（开发者调用）
    virtual bool   Write  (const void *data, VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void * Map    (VkDeviceSize offset, VkDeviceSize size) = 0;
    virtual void   Unmap  () = 0;

    // 脏标记（只记录，不触发任何提交）
    virtual void   MarkDirty  (VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE) = 0;
    virtual void   MarkDirtyRanges(const DirtyRange *ranges, size_t count)
    {
        if (!ranges || count == 0)
            return;

        for (size_t i = 0; i < count; ++i)
            MarkDirty(ranges[i].offset, ranges[i].size);
    }
    virtual bool   IsDirty    () const = 0;
    virtual void   ClearDirty () = 0;

    // ECS System 调用：将 staging 数据复制到 device buffer
    virtual void   CopyToDevice(VkCommandBuffer cmd) = 0;

    // 基本信息
    virtual VkDeviceSize  GetSize() const = 0;
    virtual VkBuffer      GetVkDeviceBuffer() const = 0;
    virtual VkDescriptorBufferInfo GetDescriptorBufferInfo() const = 0;
};//class IGPUBuffer

}//namespace hgl::graph
