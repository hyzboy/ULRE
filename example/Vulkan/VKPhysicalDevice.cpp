#include"VKPhysicalDevice.h"
#include"VKDevice.h"

VK_NAMESPACE_BEGIN

PhysicalDevice::PhysicalDevice(VkPhysicalDevice pd)
{
    physical_device=pd;

    if(!pd)return;

    vkGetPhysicalDeviceFeatures(physical_device,&features);
    vkGetPhysicalDeviceProperties(physical_device,&properties);
    vkGetPhysicalDeviceMemoryProperties(physical_device,&memory_properties);

    {
        uint32_t family_count;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,nullptr);
        family_properties.SetCount(family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device,&family_count,family_properties.GetData());
    }
}

PhysicalDevice::~PhysicalDevice()
{
}

int PhysicalDevice::QueueFamilyProperties(VkQueueFlags flag) const
{
    const int count=family_properties.GetCount();

    if(count<=0)
        return(-1);

    VkQueueFamilyProperties *fp=family_properties.GetData();
    for(int i=0;i<count;i++)
    {
        if(fp->queueFlags&flag)
            return i;

        ++fp;
    }

    return -1;
}

Device *PhysicalDevice::CreateGraphicsDevice() const
{
    const int index=QueueFamilyProperties(VK_QUEUE_GRAPHICS_BIT);

    if(index==-1)
        return(nullptr);

    float queue_priorities[1] = {0.0};

    VkDeviceQueueCreateInfo queue_info;
    queue_info.queueFamilyIndex=index;
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext = nullptr;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = queue_priorities;

    VkDeviceCreateInfo create_info = {};
    const char *ext_list[1]={VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.queueCreateInfoCount = 1;
    create_info.pQueueCreateInfos = &queue_info;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = ext_list;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;
    create_info.pEnabledFeatures = nullptr;

    VkDevice device;
    VkResult res = vkCreateDevice(physical_device, &create_info, nullptr, &device);
    if(res != VK_SUCCESS)
        return(nullptr);

    return(new Device(device,index));
}

VK_NAMESPACE_END
