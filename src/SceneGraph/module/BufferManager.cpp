#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

void BufferManager::AddBuffer(const AnsiString &buf_name, DeviceBuffer *buf)
{
    rm_buffers.Add(buf);

#ifdef _DEBUG
    DebugUtils *du = GetDevice()->GetDebugUtils();

    if (du)
    {
        du->SetBuffer(buf->GetBuffer(), buf_name + ":Buffer");
        du->SetDeviceMemory(buf->GetVkMemory(), buf_name + ":Memory");
    }
#endif//_DEBUG
}

GRAPH_MODULE_CONSTRUCT(BufferManager)
{
}

VAB *BufferManager::CreateVAB(VkFormat format, uint32_t count, const void *data, SharingMode sharing_mode)
{
    VulkanDevice *device = GetDevice();
    VAB *vb = device->CreateVAB(format, count, data, sharing_mode);

    if (!vb)
        return(nullptr);

    rm_buffers.Add(vb);

    return vb;
}

#define BUFFER_MANAGER_CREATE_BUFFER(name)    DeviceBuffer *BufferManager::Create##name(const AnsiString &buf_name, VkDeviceSize size, void *data, SharingMode sharing_mode) \
                                              {   \
                                                  VulkanDevice *device = GetDevice(); \
                                                  DeviceBuffer *buf = device->Create##name(size, data, sharing_mode);   \
                                                  \
                                                  if (!buf) return(nullptr);    \
                                                  AddBuffer(#name ":" + buf_name, buf);    \
                                                  return(buf);    \
                                              }

BUFFER_MANAGER_CREATE_BUFFER(UBO)
BUFFER_MANAGER_CREATE_BUFFER(SSBO)
BUFFER_MANAGER_CREATE_BUFFER(INBO)

#undef BUFFER_MANAGER_CREATE_BUFFER

IndexBuffer *BufferManager::CreateIBO(IndexType index_type, uint32_t count, const void *data, SharingMode sharing_mode)
{
    VulkanDevice *device = GetDevice();
    IndexBuffer *buf = device->CreateIBO(index_type, count, data, sharing_mode);

    if (!buf) return(nullptr);
    rm_buffers.Add(buf);
    return(buf);
}

VK_NAMESPACE_END