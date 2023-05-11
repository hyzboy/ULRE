#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN
struct VulkanHardwareRequirement
{
    uint min_1d_image_size;
    uint min_2d_image_size;
    uint min_3d_image_size;
    uint min_cube_image_size;
    uint min_array_image_layers;

    uint min_vertex_input_attribute;            ///<最小顶点输入属性数量需求
    uint min_color_attachments;                 ///<最小颜色输出成份数量需求

    uint min_push_constant_size;                ///<最小push constant大小
    uint min_ubo_range;                         ///<最小ubo range需求
    uint min_ssbo_range;                        ///<最小ssbo range需求

    uint min_draw_indirect_count;               ///<最小间接绘制次数需求

    bool geometry_shader;                       ///<要求支持几何着色器
    bool tessellation_shader;                   ///<要求支持细分着色器
//    bool compute_shader;                        ///<要求支持计算着色器

    bool multi_draw_indirect;                   ///<要求支持MultiDrawIndirect

    bool wide_lines;                            ///<要求支持宽线条
    bool line_rasterization;                    ///<要支持线条特性(这功能mac/ios平台不支持)
    bool large_points;                          ///<要求支持绘制大点

    bool texture_cube_array;                    ///<要求支持立方体数组纹理

    bool uint8_draw_index;                      ///<要求支持8位索引
    bool uint32_draw_index;                     ///<要求支持32位索引(不建议使用)

    struct
    {
        bool bc,etc2,astc_ldr,astc_hdr,pvrtc;   ///<要求支持的压缩纹理格式
    }texture_compression;

    //dynamic_state VK_EXT_extended_dynamic_state
    //  cull mode
    //  front face
    //  primitive topology
    //  viewport
    //  scissor
    //  bind vbo
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
    bool dynamic_state[3];                      ///<要求支持动态状态

    // 1.3 特性
    bool dynamic_rendering;                     ///<要求支持动态渲染

    uint32_t descriptor_pool;                   ///<描述符池大小(默认1024)

public:

    VulkanHardwareRequirement()
    {
        hgl_zero(*this);

        descriptor_pool=1024;
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
    PF_RGBA8UN,PF_RGBA8s,
    PF_BGRA8UN,PF_BGRA8s,
    PF_ABGR8UN,PF_ABGR8s,
    PF_A2RGB10UN,
    PF_A2BGR10UN,
    PF_B10GR11UF
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

constexpr const VkFormat SwapchainPreferFormatsDepth[]=
{
    PF_D16UN,
    PF_X8_D24UN,
    PF_D16UN_S8U,
    PF_D24UN_S8U,
    PF_D32F,
    PF_D32F_S8U
};

struct PreferFormats
{
    //偏好格式需从低质量到高质量排列
    const VkFormat *formats;
    uint count;
};

constexpr const PreferFormats PreferLDR  {SwapchainPreferFormatsLDR,     sizeof(SwapchainPreferFormatsLDR    )/sizeof(VkFormat)};
constexpr const PreferFormats PreferSDR  {SwapchainPreferFormatsSDR,     sizeof(SwapchainPreferFormatsSDR    )/sizeof(VkFormat)};
constexpr const PreferFormats PreferHDR16{SwapchainPreferFormatsHDR16,   sizeof(SwapchainPreferFormatsHDR16  )/sizeof(VkFormat)};
constexpr const PreferFormats PreferHDR32{SwapchainPreferFormatsHDR32,   sizeof(SwapchainPreferFormatsHDR32  )/sizeof(VkFormat)};
constexpr const PreferFormats PreferDepth{SwapchainPreferFormatsDepth,   sizeof(SwapchainPreferFormatsDepth  )/sizeof(VkFormat)};

/**
* Vulkan设备创建器
*/
class VulkanDeviceCreater
{
protected:

    VulkanInstance *instance;
    Window *window;
    const GPUPhysicalDevice *physical_device;

    VulkanHardwareRequirement require;

    VkExtent2D extent;

    const PreferFormats *perfer_color_formats;
    const PreferFormats *perfer_depth_formats;

    VkSurfaceKHR surface;

    CharPointerList ext_list;
    VkPhysicalDeviceFeatures features={};

protected:

    VkDevice CreateDevice(const uint32_t);

public:

    VulkanDeviceCreater(VulkanInstance *vi,
                        Window *win,
                        const PreferFormats *spf_color,
                        const PreferFormats *spf_depth,
                        const VulkanHardwareRequirement *req);

    virtual bool ChoosePhysicalDevice()
    {
        physical_device=nullptr;

        if(!physical_device)physical_device=instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);      //先找独显
        if(!physical_device)physical_device=instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU);    //再找集显
        if(!physical_device)physical_device=instance->GetDevice(VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU);       //最后找虚拟显卡
        
        return physical_device;
    }

    virtual bool RequirementCheck();

    virtual GPUDevice *CreateRenderDevice();

public:

    virtual GPUDevice *Create();
};//class VulkanDeviceCreater

inline GPUDevice *CreateRenderDevice(   VulkanInstance *vi,
                                        Window *win,
                                        const PreferFormats *spf_color=&PreferSDR,
                                        const PreferFormats *spf_depth=&PreferDepth,
                                        const VulkanHardwareRequirement *req=nullptr)
{
    VulkanDeviceCreater vdc(vi,win,spf_color,spf_depth,req);

    return vdc.Create();
}
VK_NAMESPACE_END
