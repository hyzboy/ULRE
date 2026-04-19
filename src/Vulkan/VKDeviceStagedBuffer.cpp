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
        // Try HOST_VISIBLE + DEVICE_LOCAL first (ideal for integrated GPU)
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                   | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        // Try to find memory type with all flags
        int index = attr->physical_device->GetMemoryType(req.memoryTypeBits, properties);
        if (index >= 0)
        {
            // Found ideal memory type, use existing CreateMemory
            return CreateMemory(req, properties, name, loc);
        }

        // Fallback to just HOST_VISIBLE + HOST_COHERENT for discrete GPU
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        break;
    }

    case MemoryUsage::GPUToCPU:
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;

    case MemoryUsage::ReBAR:
        // Resizable BAR: HOST_VISIBLE + HOST_COHERENT + DEVICE_LOCAL
        // This provides optimal performance when ReBAR is available
        properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                   | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        // Try to find memory type with all flags
        {
            int index = attr->physical_device->GetMemoryType(req.memoryTypeBits, properties);
            if (index >= 0)
            {
                // ReBAR is available, use it
                return CreateMemory(req, properties, name, loc);
            }
        }

        // ReBAR not available, fallback to staging pattern (CPUToGPU)
        // This ensures the code still works on systems without ReBAR
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

    // Create staging buffer (CPU accessible)
    BufferCreateInfo staging_info;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_info.size = size;
    staging_info.queueFamilyIndexCount = 0;
    staging_info.pQueueFamilyIndices = nullptr;
    staging_info.sharingMode = VkSharingMode(sharing_mode);

    VkBuffer staging_buffer;
    if (vkCreateBuffer(attr->device, &staging_info, nullptr, &staging_buffer) != VK_SUCCESS)
        return nullptr;

    VkMemoryRequirements staging_mem_reqs;
    vkGetBufferMemoryRequirements(attr->device, staging_buffer, &staging_mem_reqs);

    ObjectNameBuilder staging_memory_name = make_child_name(name, "StagingMemory");
    DeviceMemory *staging_memory = CreateMemory(staging_mem_reqs, MemoryUsage::CPUToGPU, staging_memory_name);
    if (!staging_memory || !staging_memory->BindBuffer(staging_buffer))
    {
        delete staging_memory;
        vkDestroyBuffer(attr->device, staging_buffer, nullptr);
        return nullptr;
    }


    // Create device buffer (GPU optimal)
    BufferCreateInfo device_info;
    device_info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT; // Add transfer dst bit
    device_info.size = size;
    device_info.queueFamilyIndexCount = 0;
    device_info.pQueueFamilyIndices = nullptr;
    device_info.sharingMode = VkSharingMode(sharing_mode);

    VkBuffer device_buffer;
    if (vkCreateBuffer(attr->device, &device_info, nullptr, &device_buffer) != VK_SUCCESS)
    {
        delete staging_memory;
        vkDestroyBuffer(attr->device, staging_buffer, nullptr);
        return nullptr;
    }

    VkMemoryRequirements device_mem_reqs;
    vkGetBufferMemoryRequirements(attr->device, device_buffer, &device_mem_reqs);

    ObjectNameBuilder device_memory_name = make_child_name(name, "DeviceMemory");
    DeviceMemory *device_memory = CreateMemory(device_mem_reqs, MemoryUsage::GPUOnly, device_memory_name);
    if (!device_memory || !device_memory->BindBuffer(device_buffer))
    {
        delete device_memory;
        delete staging_memory;
        vkDestroyBuffer(attr->device, device_buffer, nullptr);
        vkDestroyBuffer(attr->device, staging_buffer, nullptr);
        return nullptr;
    }

    ObjectNameBuilder staging_buffer_name = make_child_name(name, "StagingBuffer");
    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)staging_buffer,
                staging_buffer_name, loc);

    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)static_cast<VkDeviceMemory>(*staging_memory),
                staging_memory_name, loc);
    ObjectNameBuilder device_buffer_name = make_child_name(name, "VkBufferOwner");
    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)device_buffer,
                device_buffer_name, loc);

    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)static_cast<VkDeviceMemory>(*device_memory),
                device_memory_name, loc);

    // Create StagedBuffer wrapper
    StagedBuffer *staged_buffer = new StagedBuffer(
        std::string(name.base_name),
        attr->device,
        staging_buffer,
        staging_memory,
        device_buffer,
        device_memory,
        size,
        usage
    );

    // If initial data provided, write it
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
