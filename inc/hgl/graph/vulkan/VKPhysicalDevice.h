#pragma once

#include<hgl/graph/vulkan/VK.h>
#include<hgl/type/BaseString.h>

VK_NAMESPACE_BEGIN
class PhysicalDevice
{
    VkInstance                          instance=nullptr;
    VkPhysicalDevice                    physical_device=nullptr;
    VkPhysicalDeviceFeatures            features;
    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceDriverPropertiesKHR driver_properties;
    VkPhysicalDeviceMemoryProperties    memory_properties;
    List<VkLayerProperties>             layer_properties;
    List<VkExtensionProperties>         extension_properties;

public:

    PhysicalDevice(VkInstance,VkPhysicalDevice);
    ~PhysicalDevice()=default;

    operator VkPhysicalDevice(){return physical_device;}
    operator const VkPhysicalDevice()const{return physical_device;}

    const bool              CheckMemoryType(uint32_t,VkMemoryPropertyFlags,uint32_t *)const;

    VkPhysicalDeviceType    GetDeviceType()const{return properties.deviceType;}
    const char *            GetDeviceName()const{return properties.deviceName;}

    const VkPhysicalDeviceFeatures &        GetFeatures         ()const{return features;}
    const VkPhysicalDeviceProperties &      GetProperties       ()const{return properties;}
    const VkPhysicalDeviceMemoryProperties &GetMemoryProperties ()const{return memory_properties;}

    const uint32_t          GetExtensionSpecVersion(const UTF8String &name)const;

    const VkDriverIdKHR     GetDriverId     ()const{return driver_properties.driverID;}
    const char *            GetDriverName   ()const{return driver_properties.driverName;}
    const char *            GetDriverInfo   ()const{return driver_properties.driverInfo;}

public:

    const uint32_t          GetConstantSize()const{return properties.limits.maxPushConstantsSize;}

public:

    /**
    * 获取该设备是否是显卡
    */
    const bool isGPU()const
    {
        if(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)return(true);
        if(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)return(true);
        if(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)return(true);

        return(false);
    }

    const bool isDiscreteGPU    ()const{return(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);}           ///<是否是独立显卡
    const bool isIntegratedGPU  ()const{return(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);}         ///<是否是集成显卡
    const bool isVirtualGPU     ()const{return(properties.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);}            ///<是否是虚拟显卡

public:

    VkFormatProperties GetFormatProperties(const VkFormat format)const
    {
        VkFormatProperties fp;

        vkGetPhysicalDeviceFormatProperties(physical_device,format,&fp);

        return fp;
    }
    
    VkFormat GetDepthFormat(bool lower_to_high=true)const;
    VkFormat GetDepthStencilFormat(bool lower_to_high=true)const;

    bool CheckDepthFormat(const VkFormat)const;
};//class PhysicalDevice
VK_NAMESPACE_END