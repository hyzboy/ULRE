#pragma once

#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VKBufferOwner.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph {

/**
 * AlignedStructureBuffer — UBO/SSBO 对齐数组缓冲区
 *
 * CN: 提供透明的 CPU 紧密布局到 GPU 对齐布局的自动转换。
 *     应用层看到的是普通的结构体数组，无需关心对齐细节。
 *
 * EN: Transparent CPU-to-GPU alignment transformation for structured arrays.
 *     Applications work with tightly-packed CPU arrays while GPU buffer maintains proper alignment.
 *
 * 设计特色 / Design Features:
 * 1. CPU 侧数据紧密存储（最优内存利用）
 * 2. GPU 侧自动对齐扩展（UBO/SSBO 合规）
 * 3. SyncToGPU() 一次性同步（包含对齐转换）
 * 4. 完全集成 IGPUBuffer 路径
 * 5. 支持 StagedBuffer/ReBarBuffer 双路由
 *
 * 典型对齐需求 / Typical Alignment:
 * - UBO/SSBO standard layout: 每个数组元素对齐到 140 字节（std430）或 256 字节（std140）
 * - 具体值由 device->GetUBOAlign() / device->GetSSBOAlign() 提供
 */
template<typename T>
class AlignedStructureBuffer {
private:
    size_t          element_count = 0;
    size_t          alignment     = 0;    ///< GPU 要求的对齐字节数

    size_t          cpu_stride    = sizeof(T);  ///< 紧密布局的步长
    size_t          gpu_stride    = sizeof(T);  ///< 对齐后的步长（默认与 cpu_stride 相同）

    uint8_t*        cpu_buffer    = nullptr;    ///< 紧密数组（CPU 写这里）
    VkBufferOwner*  owned_buf     = nullptr;    ///< 所有者，析构时 delete
    IGPUBuffer*     gpu_buffer    = nullptr;    ///< 写路径接口（由 owned_buf 持有，不独立 delete）

    bool            dirty         = false;

public:
    /**
     * 构造函数
     * @param device       Vulkan 设备
     * @param count        数组元素个数
     * @param align        GPU 对齐要求（字节），传 0 则不做对齐
     * @param buffer_usage VkBufferUsageFlags
     * @param alloc_policy 分配策略（默认 CPUVisible）
     */
    AlignedStructureBuffer(
        VulkanDevice*      device,
        size_t             count,
        size_t             align,
        VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        BufferAllocPolicy  alloc_policy = BufferAllocPolicy::CPUVisible)
        : element_count(count)
        , alignment(align)
    {
        if (element_count == 0 || !device)
            return;

        // 计算 GPU 侧对齐后的步长
        // 例：sizeof(T)=112, alignment=256 => gpu_stride=256
        if (alignment > 0)
            gpu_stride = ((sizeof(T) + alignment - 1) / alignment) * alignment;
        else
            gpu_stride = sizeof(T);

        // CPU 缓冲：紧密数组，值初始化为 0
        cpu_buffer = new uint8_t[element_count * cpu_stride]();

        // GPU 缓冲：对齐数组
        const VkDeviceSize gpu_total_size = static_cast<VkDeviceSize>(element_count) * gpu_stride;

        owned_buf = device->CreateBuffer(buffer_usage, gpu_total_size, nullptr, alloc_policy);
        if (owned_buf)
            gpu_buffer = owned_buf->GetGPUBuffer();
    }

    ~AlignedStructureBuffer()
    {
        gpu_buffer = nullptr;
        delete owned_buf;
        owned_buf = nullptr;

        delete[] cpu_buffer;
        cpu_buffer = nullptr;
    }

    // 禁止拷贝 / Disable copy
    AlignedStructureBuffer(const AlignedStructureBuffer&) = delete;
    AlignedStructureBuffer& operator=(const AlignedStructureBuffer&) = delete;

public:

    // ============ CPU 侧访问（完全透明）============

    /// 获取紧密数组基址（应用直接在此写入）
    T* GetData()
    {
        dirty = true;
        return reinterpret_cast<T*>(cpu_buffer);
    }

    /// 下标访问
    T& operator[](size_t index)
    {
        dirty = true;
        if (index >= element_count)
            index = element_count - 1; // 夹紧到末尾，避免越界崩溃

        return *reinterpret_cast<T*>(cpu_buffer + index * cpu_stride);
    }

    const T& operator[](size_t index) const
    {
        if (index >= element_count)
            index = element_count - 1;

        return *reinterpret_cast<const T*>(cpu_buffer + index * cpu_stride);
    }

    /// 获取元素个数
    size_t GetCount()      const { return element_count; }

    /// 获取单个元素大小（紧密大小）
    size_t GetElementSize() const { return sizeof(T); }

    /// 获取 GPU 对齐后的步长
    size_t GetGPUStride()  const { return gpu_stride; }

    /// 检查是否有效
    bool IsValid() const { return gpu_buffer != nullptr && element_count > 0; }
    operator bool() const { return IsValid(); }

    // ============ 同步流程：CPU → GPU ============

    /**
     * 一次性同步 CPU 紧密数组到 GPU 对齐缓冲区
     * 内部自动处理对齐扩展和填充
     * @return 成功返回 true
     */
    bool SyncToGPU()
    {
        if (!dirty || !gpu_buffer || !cpu_buffer || element_count == 0)
            return true;  // 无脏数据或无效缓冲

        void *gpu_ptr = gpu_buffer->Map(0, static_cast<VkDeviceSize>(element_count) * gpu_stride);
        if (!gpu_ptr)
            return false;

        // 逐元素复制 + 对齐填充
        for (size_t i = 0; i < element_count; ++i)
        {
            const void *src = cpu_buffer + i * cpu_stride;
            void       *dst = reinterpret_cast<uint8_t*>(gpu_ptr) + i * gpu_stride;

            memcpy(dst, src, cpu_stride);

            // 对齐填充部分（清零确保不含垃圾值）
            if (gpu_stride > cpu_stride)
                memset(reinterpret_cast<uint8_t*>(dst) + cpu_stride, 0, gpu_stride - cpu_stride);
        }

        gpu_buffer->Unmap();
        dirty = false;
        return true;
    }

    /// 标记为 dirty（下次 SyncToGPU 时才会上传）
    void MarkDirty()  { dirty = true; }

    /// 清除脏标志
    void ClearDirty() { dirty = false; }

    /// 检查是否有未同步的改动
    bool IsDirty() const { return dirty; }

    // ============ GPU 侧绑定 ============

    /// 获取 GPU 缓冲写路径接口（用于手动操作）
    IGPUBuffer* GetGPUBuffer() const { return gpu_buffer; }

    /// 获取底层 VkBufferOwner（用于描述符绑定等）
    VkBufferOwner* GetBufferOwner() const { return owned_buf; }

    /// 获取 VkBuffer 句柄（低级访问）
    VkBuffer GetVkBuffer() const
    {
        return owned_buf ? owned_buf->GetBuffer() : VK_NULL_HANDLE;
    }

    /// 获取描述符缓冲信息
    const VkDescriptorBufferInfo* GetBufferInfo() const
    {
        return owned_buf ? owned_buf->GetBufferInfo() : nullptr;
    }

    /// 获取 GPU 缓冲总大小（对齐后）
    VkDeviceSize GetGPUBufferSize() const
    {
        return static_cast<VkDeviceSize>(element_count) * gpu_stride;
    }
};

}  // namespace hgl::graph
