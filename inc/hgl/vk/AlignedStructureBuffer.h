#pragma once

#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VKBufferOwner.h>
#include<vector>
#include<cstring>

namespace hgl::graph {

/**
 * AlignedStructureBuffer — UBO/SSBO 对齐数组缓冲区
 *
 * CN: 作为 VulkanArrayBuffer 的完全重设计，提供透明的 CPU 紧密布局到 GPU 对齐布局的自动转换。
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
    size_t                  element_count;
    size_t                  alignment;           ///< GPU 要求的对齐字节数

    size_t                  cpu_stride = sizeof(T);   ///< 紧密布局的步长
    size_t                  gpu_stride;               ///< 对齐后的步长

    std::vector<uint8_t>    cpu_buffer;          ///< 紧密数组（CPU 写这里）
    IGPUBuffer*             gpu_buffer = nullptr;    ///< 对齐数组（GPU 读这里）

    bool                    dirty = false;

public:
    /**
     * 构造函数
     * @param device     Vulkan 设备
     * @param count      数组元素个数
     * @param alignment  GPU 对齐要求（字节）
     * @param buffer_usage VkBufferUsageFlags（通常 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 或 _STORAGE_BUFFER_BIT）
     * @param alloc_policy 分配策略（默认 CPUVisible）
     */
    AlignedStructureBuffer(
        VulkanDevice* device,
        size_t count,
        size_t alignment,
        VkBufferUsageFlags buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        BufferAllocPolicy alloc_policy = BufferAllocPolicy::CPUVisible)
        : element_count(count), alignment(alignment)
    {
        if(element_count == 0 || alignment == 0)
            return;  // 无效的初始化，gpu_buffer 保持 nullptr

        // 计算 GPU 侧对齐后的步长
        // 例：sizeof(T)=112, alignment=256 => gpu_stride=256
        gpu_stride = ((sizeof(T) + alignment - 1) / alignment) * alignment;

        // CPU 缓冲：紧密数组
        cpu_buffer.resize(element_count * cpu_stride);

        // GPU 缓冲：对齐数组（使用 IGPUBuffer 路径）
        const VkDeviceSize gpu_total_size = element_count * gpu_stride;

        VkBufferOwner* buf = device->CreateBuffer(
            buffer_usage,
            gpu_total_size,
            nullptr,  // no initial data
            alloc_policy
        );

        if(buf) {
            gpu_buffer = buf->GetGPUBuffer();
        }
    }

    ~AlignedStructureBuffer() {
        // gpu_buffer 的生命周期由 VkBufferOwner/DeviceBuffer 管理，不在这里删除
        gpu_buffer = nullptr;
    }

    // ============ CPU 侧访问（完全透明）============

    /// 获取紧密数组基址（应用直接在此写入）
    T* GetData() {
        dirty = true;
        return reinterpret_cast<T*>(cpu_buffer.data());
    }

    /// 下标访问（RAII 风格）
    T& operator[](size_t index) {
        dirty = true;
        if(index >= element_count)
            throw std::out_of_range("AlignedStructureBuffer index out of range");
        return *reinterpret_cast<T*>(cpu_buffer.data() + index * cpu_stride);
    }

    const T& operator[](size_t index) const {
        if(index >= element_count)
            throw std::out_of_range("AlignedStructureBuffer index out of range");
        return *reinterpret_cast<const T*>(cpu_buffer.data() + index * cpu_stride);
    }

    /// 获取元素个数
    size_t GetCount() const { return element_count; }

    /// 获取单个元素大小（紧密大小）
    size_t GetElementSize() const { return sizeof(T); }

    /// 获取 GPU 对齐后的步长
    size_t GetGPUStride() const { return gpu_stride; }

    /// 检查是否有效
    bool IsValid() const { return gpu_buffer != nullptr && element_count > 0; }

    // ============ 同步流程：CPU → GPU ============

    /**
     * 一次性同步 CPU 紧密数组到 GPU 对齐缓冲区
     * 内部自动处理对齐扩展和填充
     * @return 成功返回 true
     */
    bool SyncToGPU() {
        if(!dirty || !gpu_buffer || element_count == 0)
            return true;  // 无脏数据或无效缓冲

        // Map GPU 缓冲
        void* gpu_ptr = gpu_buffer->Map(0, element_count * gpu_stride);
        if(!gpu_ptr) {
            return false;
        }

        // 逐元素复制 + 对齐填充
        for(size_t i = 0; i < element_count; ++i) {
            const void* src = cpu_buffer.data() + i * cpu_stride;
            void* dst = reinterpret_cast<uint8_t*>(gpu_ptr) + i * gpu_stride;

            // 复制有效数据
            std::memcpy(dst, src, cpu_stride);

            // 对齐填充部分（清零确保不含垃圾值）
            if(gpu_stride > cpu_stride) {
                std::memset(
                    reinterpret_cast<uint8_t*>(dst) + cpu_stride,
                    0,
                    gpu_stride - cpu_stride
                );
            }
        }

        // Unmap 时自动标记 dirty（由 IGPUBuffer::Unmap() 负责）
        gpu_buffer->Unmap();

        dirty = false;
        return true;
    }

    /// 清除脏标志
    void ClearDirty() {
        dirty = false;
    }

    /// 检查是否有未同步的改动
    bool IsDirty() const {
        return dirty;
    }

    // ============ GPU 侧绑定 ============

    /// 获取 GPU 缓冲接口（用于 BindUBO/BindSSBO）
    IGPUBuffer* GetGPUBuffer() const {
        return gpu_buffer;
    }

    /// 获取 VkBuffer（低级访问）
    VkBuffer GetVkBuffer() const {
        return gpu_buffer ? gpu_buffer->GetVkDeviceBuffer() : VK_NULL_HANDLE;
    }

    /// 获取描述符缓冲信息
    VkDescriptorBufferInfo GetBufferInfo() const {
        if(gpu_buffer) {
            return gpu_buffer->GetBufferInfo();
        }
        return VkDescriptorBufferInfo{VK_NULL_HANDLE, 0, 0};
    }

    /// 获取总大小（对齐后）
    VkDeviceSize GetGPUBufferSize() const {
        return element_count * gpu_stride;
    }
};

}  // namespace hgl::graph
