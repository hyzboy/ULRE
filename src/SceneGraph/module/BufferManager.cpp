#include<hgl/graph/module/BufferManager.h>
#include<hgl/vk/VKDevice.h>
#include <cstdint>

VK_NAMESPACE_BEGIN

void BufferManager::AddBuffer(const AnsiString &buf_name, DeviceBuffer *buf, const std::source_location &loc)
{
    rm_buffers.Add(buf);

    VulkanDevice *device = GetDevice();
    if (device)
        device->TrackBuffer(buf, buf_name, loc);
}

GRAPH_MODULE_CONSTRUCT(BufferManager)
{
}

VAB *BufferManager::CreateVAB(VkFormat format, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sharing_mode, const std::source_location &loc)
{
    VulkanDevice *device = GetDevice();
    if (!device)
        return nullptr;

    VAB *vb = device->CreateVAB(format, count, data, policy, sharing_mode, BufferUpdateClass::Default, loc);

    if (!vb)
        return nullptr;

    rm_buffers.Add(vb);

    AnsiString name = "VAB_" + AnsiString::numberOf(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(vb)));
    device->TrackBuffer(vb, name, loc);

    return vb;
}

#define BUFFER_MANAGER_CREATE_BUFFER(name)    DeviceBuffer *BufferManager::Create##name(const AnsiString &buf_name, VkDeviceSize size, void *data, SharingMode sharing_mode, const std::source_location &loc) \
                                              {   \
                                                  VulkanDevice *device = GetDevice(); \
                                                  DeviceBuffer *buf = device->Create##name(buf_name, size, data, BufferAllocPolicy::Auto, sharing_mode, BufferUpdateClass::Default, loc);   \
                                                  \
                                                  if (!buf) return(nullptr);    \
                                                  AddBuffer(#name ":" + buf_name, buf, loc);    \
                                                  return(buf);    \
                                              }

BUFFER_MANAGER_CREATE_BUFFER(UBO)
BUFFER_MANAGER_CREATE_BUFFER(SSBO)
BUFFER_MANAGER_CREATE_BUFFER(INBO)

#undef BUFFER_MANAGER_CREATE_BUFFER

IndexBuffer *BufferManager::CreateIBO(IndexType index_type, uint32_t count, const void *data, BufferAllocPolicy policy, SharingMode sharing_mode, const std::source_location &loc)
{
    VulkanDevice *device = GetDevice();
    if (!device)
        return nullptr;

    IndexBuffer *buf = device->CreateIBO(index_type, count, data, policy, sharing_mode, BufferUpdateClass::Default, loc);

    if (!buf)
        return nullptr;

    rm_buffers.Add(buf);
    return buf;
}

VK_NAMESPACE_END
