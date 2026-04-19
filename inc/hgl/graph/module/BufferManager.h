#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKBufferOwner.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/type/ObjectManager.h>
#include<hgl/log/Log.h>
#include<source_location>

namespace hgl::graph{

using BufferID = int;

GRAPH_MODULE_CLASS(BufferManager)
{
private:

    AutoIdObjectManager<BufferID, VkBufferOwner> rm_buffers;                 ///<缓冲区合集

    // TODO: Split into specialized sub-managers (UBO/SSBO/VBO/IBO) while keeping BufferManager as the single entry point.

    void AddBuffer(const AnsiString &buf_name, VkBufferOwner *buf, const std::source_location &loc);

private:

    BufferManager(GraphicsContext *);
    ~BufferManager() = default;

    friend class GraphModuleManager;

public: //Add/Get/Release

    BufferID        Add(VkBufferOwner *buf) { return rm_buffers.Add(buf); }
    VkBufferOwner * Get(const BufferID &id) { return rm_buffers.Get(id); }
    void            Release(VkBufferOwner *buf) { rm_buffers.Release(buf, true); }

    /**
     * @brief 清理所有残留的缓冲区，防止销毁设备时出现资源泄漏
     * 此方法由GraphModuleManager自动调用，用于清理模块持有的所有GPU资源
     * @warning 必须在销毁设备之前调用
     */
    void            ClearAllBuffers() { rm_buffers.Clear(); }

public:

    void Release() override
    {
        // 清理所有残留的缓冲区
        // 如果Release()被调用时还有缓冲区未清理，说明有资源泄漏
        VulkanDevice *device = GetDevice();
        const int buffer_count = rm_buffers.GetCount();

        if (device && device->GetTrackedObjectCount() > 0)
        {
            GLogWarning("[BufferManager::Release] WARNING: Found tracked Vulkan objects still alive before cleanup.");
            device->DumpTrackedObjects();
        }

        if (buffer_count > 0)
        {
            GLogWarning("[BufferManager::Release] WARNING: Found %d undestroyed buffers, cleaning up...", buffer_count);
            ClearAllBuffers();
            GLogWarning("[BufferManager::Release] Cleanup complete");
        }
    }

public: // VAB/VAO

    VAB *CreateVAB(VkFormat format, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    VAB *CreateVAB(const ObjectNameBuilder &name, VkFormat format, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    VAB *CreateVAB(VkFormat format, uint32_t count, const void *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateVAB(format, count, data, BufferAllocPolicy::Auto, sm, loc); }
    VAB *CreateVAB(VkFormat format, uint32_t count, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateVAB(format, count, nullptr, BufferAllocPolicy::Auto, sm, loc); }

public: // Buffer creation methods

    #define BUFFER_MANAGER_CREATE_FUNC(name)  VKDescriptorBuffer *Create##name(const AnsiString &buf_name, VkDeviceSize size, void *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());   \
                                              VKDescriptorBuffer *Create##name(const AnsiString &buf_name, VkDeviceSize size, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return Create##name(buf_name, size, nullptr, sm, loc); }

    BUFFER_MANAGER_CREATE_FUNC(UBO)
    BUFFER_MANAGER_CREATE_FUNC(SSBO)
    BUFFER_MANAGER_CREATE_FUNC(INBO)

    #undef BUFFER_MANAGER_CREATE_FUNC

    template<typename UBOAccessorType>
    UBOAccessorType *CreateUBO(SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current())
    {
        const char *name = UBOAccessorType::GetDefaultDescriptorName();
        if (!name || !*name)
            return nullptr;

        constexpr BufferUpdateClass update_class = UBOAccessorType::GetDefaultUpdateClass();
        constexpr BufferAllocPolicy alloc_policy =
            update_class == BufferUpdateClass::CriticalPerFrame
                ? BufferAllocPolicy::CPUVisible
                : BufferAllocPolicy::Auto;

        VKDescriptorBuffer *buf = GetDevice()->CreateUBO(name,
                                                   static_cast<VkDeviceSize>(UBOAccessorType::GetSize()),
                                                   nullptr,
                                                   alloc_policy,
                                                   sm,
                                                   update_class,
                                                   loc);
        if (!buf)
            return nullptr;

        UBOAccessorType *accessor = UBOAccessorType::Create(buf, false);
        if (!accessor)
        {
            Release(buf);
            return nullptr;
        }

        return accessor;
    }

public: // Index Buffer creation

    IndexBuffer *CreateIBO(const ObjectNameBuilder &name, IndexType index_type, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    IndexBuffer *CreateIBO(IndexType index_type, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current());
    IndexBuffer *CreateIBO(IndexType index_type, uint32_t count, const void *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(index_type, count, data, BufferAllocPolicy::Auto, sm, loc); }
    IndexBuffer *CreateIBO8(uint32_t count, const uint8 *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(IndexType::U8, count, (void *)data, sm, loc); }
    IndexBuffer *CreateIBO16(uint32_t count, const uint16 *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(IndexType::U16, count, (void *)data, sm, loc); }
    IndexBuffer *CreateIBO32(uint32_t count, const uint32 *data, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(IndexType::U32, count, (void *)data, sm, loc); }

    IndexBuffer *CreateIBO(IndexType index_type, uint32_t count, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(index_type, count, nullptr, sm, loc); }
    IndexBuffer *CreateIBO8(uint32_t count, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(IndexType::U8, count, nullptr, sm, loc); }
    IndexBuffer *CreateIBO16(uint32_t count, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(IndexType::U16, count, nullptr, sm, loc); }
    IndexBuffer *CreateIBO32(uint32_t count, SharingMode sm = SharingMode::Exclusive, const std::source_location &loc = std::source_location::current()) { return CreateIBO(IndexType::U32, count, nullptr, sm, loc); }

};//class BufferManager

}//namespace hgl::graph
