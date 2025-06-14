#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/String.h>

VK_NAMESPACE_BEGIN
class VulkanPhyDevice
{
    VkInstance                          instance=nullptr;
    VkPhysicalDevice                    physical_device=nullptr;

    VkPhysicalDeviceFeatures            features;
    VkPhysicalDeviceVulkan11Features    features11;
    VkPhysicalDeviceVulkan12Features    features12;
    VkPhysicalDeviceVulkan13Features    features13;
    VkPhysicalDeviceVulkan14Features    features14;

    VkPhysicalDeviceProperties          properties;
    VkPhysicalDeviceVulkan11Properties  properties11;
    VkPhysicalDeviceVulkan12Properties  properties12;
    VkPhysicalDeviceVulkan13Properties  properties13;
    VkPhysicalDeviceVulkan14Properties  properties14;

    VkPhysicalDeviceMemoryProperties    memory_properties;

    ArrayList<VkLayerProperties>        layer_properties;
    ArrayList<VkExtensionProperties>    extension_properties;
    ArrayList<VkQueueFamilyProperties>  queue_family_properties;

private:

    bool support_u8_index=false;
    bool dynamic_state=false;
    
public:    

    const VkPhysicalDeviceFeatures &        GetFeatures10       ()const{return features;}
    const VkPhysicalDeviceVulkan11Features &GetFeatures11       ()const{return features11;}
    const VkPhysicalDeviceVulkan12Features &GetFeatures12       ()const{return features12;}
    const VkPhysicalDeviceVulkan13Features &GetFeatures13       ()const{return features13;}
    const VkPhysicalDeviceVulkan14Features &GetFeatures14       ()const{return features14;}

    const VkPhysicalDeviceProperties &      GetProperties       ()const{return properties;}
    const VkPhysicalDeviceMemoryProperties &GetMemoryProperties ()const{return memory_properties;}
    const VkPhysicalDeviceLimits &          GetLimits           ()const{return properties.limits;}
    
public:

    VulkanPhyDevice(VkInstance,VkPhysicalDevice);
    ~VulkanPhyDevice()=default;

    operator        VkPhysicalDevice()      {return physical_device;}
    operator const  VkPhysicalDevice()const {return physical_device;}

    const uint32_t          GetVulkanVersion()const{return properties.apiVersion;}

    const int               GetMemoryType(uint32_t,VkMemoryPropertyFlags)const;

    VkPhysicalDeviceType    GetDeviceType()const{return properties.deviceType;}
    const char *            GetDeviceName()const{return properties.deviceName;}
    
    const bool              GetLayerVersion(const AnsiString &,uint32_t &spec,uint32_t &impl)const;
    const uint32_t          GetExtensionVersion(const AnsiString &name)const;
    const bool              CheckExtensionSupport(const AnsiString &name)const;

    const uint32_t          GetUBORange     ()const{return properties.limits.maxUniformBufferRange;}
    const VkDeviceSize      GetUBOAlign     ()const{return properties.limits.minUniformBufferOffsetAlignment;}

    const uint32_t          GetSSBORange    ()const{return properties.limits.maxStorageBufferRange;}
    const VkDeviceSize      GetSSBOAlign    ()const{return properties.limits.minStorageBufferOffsetAlignment;}

    const uint32_t          GetConstantSize ()const{return properties.limits.maxPushConstantsSize;}

    const VkDeviceSize      GetMaxBufferSize()const{return properties13.maxBufferSize;}

    const uint32_t          GetMaxPushDescriptors()const{return properties14.maxPushDescriptors;}

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

#define HGL_VK_IS_BRAND(name)   (hgl::stricmp(properties.deviceName,#name,sizeof(#name))==0)

    const bool isMicrosoft  ()const{return HGL_VK_IS_BRAND(Microsoft);}
    const bool isMesa       ()const{return HGL_VK_IS_BRAND(Mesa     );}
    const bool isAMD        ()const{return HGL_VK_IS_BRAND(AMD      )
                                         ||HGL_VK_IS_BRAND(ATI      )
                                         ||HGL_VK_IS_BRAND(Radeon   );}
    const bool isNvidia     ()const{return HGL_VK_IS_BRAND(nVidia   )
                                         ||HGL_VK_IS_BRAND(GeForce  )
                                         ||HGL_VK_IS_BRAND(Quadro   )
                                         ||HGL_VK_IS_BRAND(TITAN    )
                                         ||HGL_VK_IS_BRAND(Tegra    );}
    const bool isIntel      ()const{return HGL_VK_IS_BRAND(Intel    );}
    const bool isQualcomm   ()const{return HGL_VK_IS_BRAND(Adreno   );}
    const bool isApple      ()const{return HGL_VK_IS_BRAND(Apple    );}
    const bool isImgTec     ()const{return HGL_VK_IS_BRAND(ImgTec   )
                                         ||HGL_VK_IS_BRAND(PowerVR  );}
    const bool isARM        ()const{return HGL_VK_IS_BRAND(Arm      )
                                         ||HGL_VK_IS_BRAND(Mali     );}

#undef HGL_VK_IS_BRAND

public:

    VkFormatProperties GetFormatProperties(const VkFormat format)const
    {
        VkFormatProperties fp;

        hgl_zero(fp);

        vkGetPhysicalDeviceFormatProperties(physical_device,format,&fp);

        return fp;
    }

    bool OptimalSupport (const VkFormat format,const VkFormatFeatureFlags flag)const{VkFormatProperties fp;vkGetPhysicalDeviceFormatProperties(physical_device,format,&fp);return fp.optimalTilingFeatures&flag;}
    bool LinearSupport  (const VkFormat format,const VkFormatFeatureFlags flag)const{VkFormatProperties fp;vkGetPhysicalDeviceFormatProperties(physical_device,format,&fp);return fp.linearTilingFeatures&flag;}
    bool BufferSupport  (const VkFormat format,const VkFormatFeatureFlags flag)const{VkFormatProperties fp;vkGetPhysicalDeviceFormatProperties(physical_device,format,&fp);return fp.bufferFeatures&flag;}
    
    bool IsColorAttachmentOptimal(const VkFormat format)const{return OptimalSupport(format,VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);}
    bool IsDepthAttachmentOptimal(const VkFormat format)const{return OptimalSupport(format,VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);}

    bool IsColorAttachmentLinear(const VkFormat format)const{return LinearSupport(format,VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT);}
    bool IsDepthAttachmentLinear(const VkFormat format)const{return LinearSupport(format,VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);}
    
    VkFormat GetDepthFormat(bool lower_to_high=false)const;
    VkFormat GetDepthStencilFormat(bool lower_to_high=false)const;
    
    const bool IsUBOTexelSupport    (const VkFormat format)const{return BufferSupport(format,VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT);}    
    const bool IsSTBSupport         (const VkFormat format)const{return BufferSupport(format,VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT);}
    const bool IsSTBAtomicSupport   (const VkFormat format)const{return BufferSupport(format,VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_ATOMIC_BIT);}
    const bool IsVBOSupport         (const VkFormat format)const{return BufferSupport(format,VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT);}

public:

    const VkBool32  SupportGeometryShader       ()const{return features.geometryShader;}
    const VkBool32  SupportTessellationShader   ()const{return features.tessellationShader;}

    const VkBool32  SupportCubeMapArray         ()const{return features.imageCubeArray;}

    const VkBool32  SupportU32Index             ()const{return features.fullDrawIndexUint32;}
    const VkBool32  SupportU8Index              ()const{return support_u8_index;}

    // support != open, so please don't direct use GetFeatures().
    // open any features in CreateDevice()&SetDeviceFeatures() functions.
    const bool      SupportMDI                  ()const
    {
        // the device is supported MDI, but MaxDrawIndirectCount is 1.

        return (features.multiDrawIndirect&&properties.limits.maxDrawIndirectCount>1);
    }

    const uint32_t  GetMaxMDICount              ()const
    {
        return properties.limits.maxDrawIndirectCount;
    }

    const uint32_t  GetMaxImage1D               ()const{return properties.limits.maxImageDimension1D;}
    const uint32_t  GetMaxImage2D               ()const{return properties.limits.maxImageDimension2D;}
    const uint32_t  GetMaxImage3D               ()const{return properties.limits.maxImageDimension3D;}
    const uint32_t  GetMaxImageCube             ()const{return properties.limits.maxImageDimensionCube;}
    const uint32_t  GetMaxImageArrayLayers      ()const{return properties.limits.maxImageArrayLayers;}
    const uint32_t  GetMaxBoundDescriptorSets   ()const{return properties.limits.maxBoundDescriptorSets;}

    const uint32_t  GetMaxVertexInputAttributes ()const{return properties.limits.maxVertexInputAttributes;}
    const uint32_t  GetMaxVertexInputBindings   ()const{return properties.limits.maxVertexInputBindings;}

    const uint32_t  GetMaxColorAttachments      ()const{return properties.limits.maxColorAttachments;}

    const VkBool32  SupportSamplerAnisotropy    ()const{return features.samplerAnisotropy;}
    const float     GetMaxSamplerAnisotropy     ()const{return properties.limits.maxSamplerAnisotropy;}
    const float     GetMaxSamplerLodBias        ()const{return properties.limits.maxSamplerLodBias;}

    const VkBool32  SupportYcbcrConversion      ()const{return features11.samplerYcbcrConversion;}
    const VkBool32  SupportClampMirrorToEdge    ()const{return features12.samplerMirrorClampToEdge;}

    const VkBool32  SupportSmoothLines          ()const{return features14.smoothLines;}
    const VkBool32  SupportStippledSmoothLines  ()const{return features14.stippledSmoothLines;}

    const void      GetPointSize(float &granularity,float &min_size,float &max_size) const
    {
        granularity =properties.limits.pointSizeGranularity;
        min_size    =properties.limits.pointSizeRange[0];
        max_size    =properties.limits.pointSizeRange[1];
    }

    const void      GetLineWidth(float &granularity,float &min_width,float &max_width) const
    {
        granularity =properties.limits.lineWidthGranularity;
        min_width   =properties.limits.lineWidthRange[0];
        max_width   =properties.limits.lineWidthRange[1];
    }

    const bool      SupportDynamicState() const {return dynamic_state;}
};//class VulkanPhyDevice
VK_NAMESPACE_END
