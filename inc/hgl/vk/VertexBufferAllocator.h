#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include<hgl/vk/buffer/BufferMemory.h>
#include<hgl/type/String.h>
#include<cstdint>
#include<source_location>

namespace hgl::graph
{
    class VulkanDevice;
    class BufferOwner;

    /**
     * 顶点/索引缓冲的**分配契约**（下层需要、上层实现）。
     *
     * 为什么需要它：`VertexDataManager`（顶点数据池）与几何数据装配都需要
     * 「创建 VAB/IBO 并在析构时归还」，但它们的实现分别位于 Vulkan 层与几何装配层。
     * 直接持有 `BufferManager*` 会让 Vulkan 层反向 include 上层模块。
     *
     * 本接口只暴露下层真正需要的那几件事，由 `BufferManager` 实现。传入
     * `std::source_location` 是为了保留原有的「按调用点记录分配来源」诊断口径 ——
     * 若在此处吞掉位置信息，泄漏告警会全部指到本层而非真正的调用点。
     */
    class VertexBufferAllocator
    {
    public:
        virtual ~VertexBufferAllocator() = default;

        virtual VulkanDevice *GetAllocatorDevice() = 0;

        /// 无名 VAB（顶点属性缓冲）
        virtual VAB *CreateAllocatorVAB(VkFormat format, uint32_t vertex_count,
                                        const std::source_location &loc = std::source_location::current()) = 0;

        /// 无名 IBO（索引缓冲）
        virtual IndexBuffer *CreateAllocatorIBO(IndexType index_type, uint32_t index_count, const void *data,
                                                const std::source_location &loc = std::source_location::current()) = 0;

        /// 具名 VAB（几何装配用；名字进调试标记）
        virtual VAB *CreateAllocatorNamedVAB(const AnsiString &name, VkFormat format, uint32_t vertex_count,
                                             const void *data, BufferAllocPolicy policy,
                                             const std::source_location &loc = std::source_location::current()) = 0;

        /// 具名 IBO（几何装配用）
        virtual IndexBuffer *CreateAllocatorNamedIBO(const AnsiString &name, IndexType index_type,
                                                     uint32_t index_count, const void *data, BufferAllocPolicy policy,
                                                     const std::source_location &loc = std::source_location::current()) = 0;

        /// 归还一条由本分配器创建的缓冲（不属于它时实现方应自行忽略/报错）
        virtual void ReleaseAllocatedBuffer(BufferOwner *buffer) = 0;
    };
}//namespace hgl::graph