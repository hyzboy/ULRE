#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/vk/VKStagedBuffer.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<cassert>

namespace hgl::graph{

DeviceMemory *VulkanDevice::CreateMemory(const VkMemoryRequirements &req, MemoryUsage usage, const ObjectNameBuilder &name, const std::source_location &loc)
{
    assert(name.base_name[0] != '\0' && "ERROR: CreateMemory(MemoryUsage) called with empty name! Check the call stack to find where.");
    uint32_t properties = 0;

    switch(usage)
    {
    case MemoryUsage::CPUOnly:
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;

    case MemoryUsage::GPUOnly:
        properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;

    case MemoryUsage::CPUToGPU:
    {
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                   | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        int index = attr->physical_device->GetMemoryType(req.memoryTypeBits, properties);
        if (index >= 0)
        {
            return CreateMemory(req, properties, name, loc);
        }

        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    }

    case MemoryUsage::GPUToCPU:
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;

    case MemoryUsage::ReBAR:
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                   | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        {
            int index = attr->physical_device->GetMemoryType(req.memoryTypeBits, properties);
            if (index >= 0)
            {
                return CreateMemory(req, properties, name, loc);
            }
        }

        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;

    default:
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    }

    return CreateMemory(req, properties, name, loc);
}

StagedBuffer *VulkanDevice::CreateStagedBuffer(const ObjectNameBuilder &name, VkBufferUsageFlags usage, VkDeviceSize size, const void *data, SharingMode sharing_mode, const std::source_location &loc)
{
    if (size <= 0)
        return nullptr;

    auto make_child_name = [](const ObjectNameBuilder &parent, const char *suffix) -> ObjectNameBuilder
    {
        if (!suffix || suffix[0] == '\0')
            return parent;

        if (parent.base_name[0] == '\0')
            return ObjectNameBuilder(suffix);

        AnsiString full(parent.base_name);
        full += ".";
        full += suffix;
        return ObjectNameBuilder(full);
    };

    BufferCreateInfo staging_info;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_info.size = size;
    staging_info.queueFamilyIndexCount = 0;
    staging_info.pQueueFamilyIndices = nullptr;
    staging_info.sharingMode = VkSharingMode(sharing_mode);

    VmaAllocationCreateInfo staging_alloc_info{};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_AUTO;
    staging_alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_allocation = VK_NULL_HANDLE;
    if (vmaCreateBuffer(vma_allocator,
                        static_cast<const VkBufferCreateInfo *>(&staging_info),
                        &staging_alloc_info,
                        &staging_buffer,
                        &staging_allocation,
                        nullptr) != VK_SUCCESS)
    {
        return nullptr;
    }

    BufferCreateInfo device_info;
    device_info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    device_info.size = size;
    device_info.queueFamilyIndexCount = 0;
    device_info.pQueueFamilyIndices = nullptr;
    device_info.sharingMode = VkSharingMode(sharing_mode);

    VmaAllocationCreateInfo device_alloc_info{};
    device_alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VkBuffer device_buffer = VK_NULL_HANDLE;
    VmaAllocation device_allocation = VK_NULL_HANDLE;
    if (vmaCreateBuffer(vma_allocator,
                        static_cast<const VkBufferCreateInfo *>(&device_info),
                        &device_alloc_info,
                        &device_buffer,
                        &device_allocation,
                        nullptr) != VK_SUCCESS)
    {
        vmaDestroyBuffer(vma_allocator, staging_buffer, staging_allocation);
        return nullptr;
    }

    VmaAllocationInfo staging_ai{};
    VmaAllocationInfo device_ai{};
    vmaGetAllocationInfo(vma_allocator, staging_allocation, &staging_ai);
    vmaGetAllocationInfo(vma_allocator, device_allocation, &device_ai);

    ObjectNameBuilder staging_memory_name = make_child_name(name, "StagingMemory");
    ObjectNameBuilder staging_buffer_name = make_child_name(name, "StagingBuffer");
    ObjectNameBuilder device_memory_name = make_child_name(name, "DeviceMemory");
    ObjectNameBuilder device_buffer_name = make_child_name(name, "VkBufferOwner");

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)staging_buffer,
                staging_buffer_name, loc);
    if (staging_ai.deviceMemory)
        TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)staging_ai.deviceMemory,
                    staging_memory_name, loc);

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)device_buffer,
                device_buffer_name, loc);
    if (device_ai.deviceMemory)
        TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)device_ai.deviceMemory,
                    device_memory_name, loc);

    StagedBuffer *staged_buffer = new StagedBuffer(
        std::string(name.base_name),
        attr->device,
        vma_allocator,
        staging_buffer,
        staging_allocation,
        staging_ai.deviceMemory,
        device_buffer,
        device_allocation,
        device_ai.deviceMemory,
        size,
        usage
    );

    if (data)
    {
        staged_buffer->Write(data, 0, size);
    }

    return staged_buffer;
}

StagedBuffer *VulkanDevice::CreateStagedBuffer(VkBufferUsageFlags usage, VkDeviceSize size, const void *data, SharingMode sharing_mode, const std::source_location &loc)
{
    return CreateStagedBuffer(ObjectNameBuilder("StagedBuffer"), usage, size, data, sharing_mode, loc);
}

}//namespace hgl::graph
