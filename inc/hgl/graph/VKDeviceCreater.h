#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKDevice.h>

VK_NAMESPACE_BEGIN

struct VulkanHardwareRequirement
{
    enum class SupportLevel
    {
        DontCare=0, ///<不介意
        Want,       ///<希望支持
        Must,       ///<必须支持
    };

    //这里使用这种大小写是为了和VulkanSDK中的定义保持一致，cpp中使用宏比较和复制时只使用一个名字即可

    uint maxImageDimension1D;
    uint maxImageDimension2D;
    uint maxImageDimension3D;
    uint maxImageDimensionCube;
    uint maxImageArrayLayers;

    uint maxVertexInputAttributes;      ///<最大顶点输入属性数量需求
    uint maxColorAttachments;           ///<最大颜色输出成份数量需求

    uint maxPushConstantsSize;          ///<最大push constant大小
    uint maxUniformBufferRange;         ///<最大ubo range需求
    uint maxStorageBufferRange;         ///<最大ssbo range需求

    uint maxDrawIndirectCount;          ///<最大间接绘制次数需求

    SupportLevel geometryShader;        ///<要求支持几何着色器
    SupportLevel tessellationShader;    ///<要求支持细分着色器

    SupportLevel sampleRateShading;     ///<要求支持采样率着色

    SupportLevel multiDrawIndirect;     ///<要求支持MultiDrawIndirect

    SupportLevel fillModeNonSolid;      ///<要求支持非实体填充模式

    SupportLevel wideLines;             ///<要求支持宽线条
    SupportLevel lineRasterization;     ///<要支持线条特性(这功能mac/ios平台不支持)
    SupportLevel largePoints;           ///<要求支持绘制大点

    SupportLevel imageCubeArray;        ///<要求支持立方体数组纹理

    SupportLevel fullDrawIndexUint8;    ///<要求支持8位索引
    SupportLevel fullDrawIndexUint32;   ///<要求支持32位索引

    struct
    {
        SupportLevel BC,ETC2,ASTC_LDR,ASTC_HDR,PVRTC;   ///<要求支持的压缩纹理格式
    }texture_compression;

    //dynamic_state VK_EXT_extended_dynamic_state
    //  cull mode
    //  front face
    //  primitive topology
    //  viewport
    //  scissor
    //  bind vab
    //  depth test
    //  depth write
    //  depth compare op
    //  depth bounds test
    //  stencil test
    //  stencil op
    //dynamic_state[1] VK_EXT_extended_dynamic_state2
    //  patch control points
    //  rasterizer discard
    //  depth bias
    //  logic op
    //  primitive restart
    //dynamic_state[2] VK_EXT_extended_dynamic_state3
    //  tess domain origin
    //  depth clamp
    //  discard polygon mode
    //  rasterization samples
    //  sample mask
    //  alpha to coverage
    //  alpha to one
    //  logic op enable
    //  color blend
    //  color blend equation
    //  color write mask
    //  depth clamp
    //  Color blend advanced
    //  line rasterization mode
    //  line stipple
    //  depth clip -1 to 1
    //  shading rate image enable
    SupportLevel dynamicState[3];              ///<要求支持动态状态

    // 1.3 特性
    SupportLevel dynamicRendering;             ///<要求支持动态渲染

    uint32_t descriptor_pool;                  ///<描述符池大小(默认1024)

public:

    VulkanHardwareRequirement()
    {
        hgl_zero(*this);

        descriptor_pool=1024;

        geometryShader=SupportLevel::Want;

        fullDrawIndexUint8=SupportLevel::Want;
        fullDrawIndexUint32=SupportLevel::Want;
    }
};

constexpr const VkFormat SwapchainPreferFormatsLDR[]=
{
    PF_RGB5A1,
    PF_BGR5A1,
    PF_A1RGB5,
    PF_RGB565,
    PF_BGR565,
};

constexpr const VkFormat SwapchainPreferFormatsSDR[]=
{
    PF_RGBA8UN,//PF_RGBA8s,
    PF_BGRA8UN,//PF_BGRA8s,
    PF_ABGR8UN,//PF_ABGR8s,
    PF_A2RGB10UN,
    PF_A2BGR10UN,
//    PF_B10GR11UF
};

constexpr const VkFormat SwapchainPreferFormatsHDR16[]=
{
    PF_RGBA16UN,PF_RGBA16SN,PF_RGBA16F
};

constexpr const VkFormat SwapchainPreferFormatsHDR32[]=
{
    PF_RGB32F,
    PF_RGBA32F
};

constexpr const VkFormat SwapchainPreferFormatsHDR[]=
{
    PF_RGBA16UN,PF_RGBA16SN,PF_RGBA16F,
    PF_RGB32F,PF_RGBA32F
};

constexpr const VkFormat SwapchainPreferFormatsDepth[]=
{
    PF_D16UN,
    PF_X8_D24UN,
    PF_D16UN_S8U,
    PF_D24UN_S8U,
    PF_D32F,
    PF_D32F_S8U
};

constexpr const VkColorSpaceKHR SwapchainPreferColorSpacesNonlinear[]=
{
    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT,
    VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT,
    VK_COLOR_SPACE_BT709_NONLINEAR_EXT,
    VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT,
    VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT,
    VK_COLOR_SPACE_DISPLAY_NATIVE_AMD,
};

constexpr const VkColorSpaceKHR SwapchainPreferColorSpacesLinear[]=
{
    VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
    VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT,
    VK_COLOR_SPACE_BT709_LINEAR_EXT,
    VK_COLOR_SPACE_BT2020_LINEAR_EXT,
    VK_COLOR_SPACE_HDR10_ST2084_EXT,
    VK_COLOR_SPACE_DOLBYVISION_EXT,
    VK_COLOR_SPACE_HDR10_HLG_EXT,
    VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT,
};

struct PreferFormats
{
    //偏好格式需从低质量到高质量排列
    const VkFormat *formats;
    uint count;

public:

    const int Find(const VkFormat fmt)const
    {
        for(uint i=0;i<count;i++)
            if(fmt==formats[i])
                return i;

        return -1;
    }
};

struct PreferColorSpaces
{
    //偏好格式需从低质量到高质量排列
    const VkColorSpaceKHR *colorspaces;
    uint count;

public:

    const int Find(const VkColorSpaceKHR cs)const
    {
        for(uint i=0;i<count;i++)
            if(cs==colorspaces[i])
                return i;

        return -1;
    }
};

constexpr const PreferFormats PreferLDR  {SwapchainPreferFormatsLDR,     sizeof(SwapchainPreferFormatsLDR    )/sizeof(VkFormat)};
constexpr const PreferFormats PreferSDR  {SwapchainPreferFormatsSDR,     sizeof(SwapchainPreferFormatsSDR    )/sizeof(VkFormat)};
constexpr const PreferFormats PreferHDR16{SwapchainPreferFormatsHDR16,   sizeof(SwapchainPreferFormatsHDR16  )/sizeof(VkFormat)};
constexpr const PreferFormats PreferHDR32{SwapchainPreferFormatsHDR32,   sizeof(SwapchainPreferFormatsHDR32  )/sizeof(VkFormat)};
constexpr const PreferFormats PreferHDR  {SwapchainPreferFormatsHDR,     sizeof(SwapchainPreferFormatsHDR    )/sizeof(VkFormat)};
constexpr const PreferFormats PreferDepth{SwapchainPreferFormatsDepth,   sizeof(SwapchainPreferFormatsDepth  )/sizeof(VkFormat)};

constexpr const PreferColorSpaces PreferNonlinear   {SwapchainPreferColorSpacesNonlinear,   sizeof(SwapchainPreferColorSpacesNonlinear  )/sizeof(VkColorSpaceKHR)};
constexpr const PreferColorSpaces PreferLinear      {SwapchainPreferColorSpacesLinear,      sizeof(SwapchainPreferColorSpacesLinear     )/sizeof(VkColorSpaceKHR)};

/**
* Vulkan设备创建器<br>
* 将此功能定义为类是为了让开发者方便重载处理
*/
class VulkanDeviceCreater
{
protected:

    VulkanInstance *instance;
    Window *window;
    const VulkanPhyDevice *physical_device;

    VulkanHardwareRequirement require;

    VkExtent2D extent;

    const PreferFormats *       perfer_color_formats;
    const PreferColorSpaces *   perfer_color_spaces;
    const PreferFormats *       perfer_depth_formats;

    VkSurfaceKHR surface;

    VkSurfaceFormatKHR surface_format;

    CharPointerList ext_list;
    VkPhysicalDeviceFeatures features={};

protected:

    VkDevice CreateDevice(const uint32_t);

public:

    VulkanDeviceCreater(VulkanInstance *vi,
                        Window *win,
                        const VulkanHardwareRequirement *req,
                        const PreferFormats *spf_color,
                        const PreferColorSpaces *spf_color_space,
                        const PreferFormats *spf_depth);

    virtual bool ChoosePhysicalDevice();

    virtual bool RequirementCheck();

    virtual void ChooseSurfaceFormat();

    virtual VulkanDevice *CreateRenderDevice();

public:

    virtual VulkanDevice *Create();
};//class VulkanDeviceCreater

inline VulkanDevice *CreateRenderDevice(   VulkanInstance *vi,
                                        Window *win,
                                        const VulkanHardwareRequirement *req=nullptr,
                                        const PreferFormats *       spf_color       =&PreferSDR,
                                        const PreferColorSpaces *   spf_color_space =&PreferNonlinear,
                                        const PreferFormats *       spf_depth       =&PreferDepth)
{
    VulkanDeviceCreater vdc(vi,win,req,spf_color,spf_color_space,spf_depth);

    return vdc.Create();
}

inline VulkanDevice *CreateRenderDeviceLDR(VulkanInstance *vi,
                                        Window *win,
                                        const VulkanHardwareRequirement *req=nullptr)
{
    return CreateRenderDevice(vi,win,req,&PreferLDR,&PreferNonlinear,&PreferDepth);
}

inline VulkanDevice *CreateRenderDeviceSDR(VulkanInstance *vi,
                                        Window *win,
                                        const VulkanHardwareRequirement *req=nullptr)
{
    return CreateRenderDevice(vi,win,req,&PreferSDR,&PreferNonlinear,&PreferDepth);
}

inline VulkanDevice *CreateRenderDeviceHDR16(  VulkanInstance *vi,
                                            Window *win,
                                            const VulkanHardwareRequirement *req=nullptr)
{
    return CreateRenderDevice(vi,win,req,&PreferHDR16,&PreferLinear,&PreferDepth);
}

inline VulkanDevice *CreateRenderDeviceHDR32(  VulkanInstance *vi,
                                            Window *win,
                                            const VulkanHardwareRequirement *req=nullptr)
{
    return CreateRenderDevice(vi,win,req,&PreferHDR32,&PreferLinear,&PreferDepth);
}

inline VulkanDevice *CreateRenderDeviceHDR(VulkanInstance *vi,
                                        Window *win,
                                        const VulkanHardwareRequirement *req=nullptr)
{
    return CreateRenderDevice(vi,win,req,&PreferHDR,&PreferLinear,&PreferDepth);
}
VK_NAMESPACE_END
