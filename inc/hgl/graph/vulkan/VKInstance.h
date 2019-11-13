#ifndef HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_INSTANCE_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKDebugOut.h>

VK_NAMESPACE_BEGIN
    #define VK_BOOL1BIT(name)   bool name:1;
    struct CreateInstanceLayerInfo
    {
        struct
        {
            VK_BOOL1BIT(api_dump)
            VK_BOOL1BIT(device_simulation)
            VK_BOOL1BIT(monitor)
            VK_BOOL1BIT(screenshot)
            VK_BOOL1BIT(standard_validation)
            VK_BOOL1BIT(vktrace)
        }lunarg;

        struct
        {
            VK_BOOL1BIT(validation)
        }khronos;

        struct
        {
            VK_BOOL1BIT(optimus)
        }nv;

        struct
        {
            VK_BOOL1BIT(steam_overlay)
            VK_BOOL1BIT(steam_fossilize)
        }valve;

        struct
        {
            VK_BOOL1BIT(Capture)
        }RenderDoc;

        struct
        {
            VK_BOOL1BIT(helper);
        }bandicam;
    };
    #undef VK_BOOL1BIT

    class Instance
    {
        VkInstance inst;

        VKDebugOut *debug_out;

        ObjectList<PhysicalDevice> physical_devices;

    private:

        friend Instance *CreateInstance(const UTF8String &app_name,VKDebugOut *out=nullptr,CreateInstanceLayerInfo *cili=nullptr);

        Instance(VkInstance,VKDebugOut *);

    public:

        virtual ~Instance();

                operator VkInstance (){return inst;}

        const   ObjectList<PhysicalDevice> &GetDeviceList       ()const {return physical_devices;}
        const   PhysicalDevice *            GetDevice           (VkPhysicalDeviceType)const;
    };//class Instance
    
            void                            InitVulkanProperties();
    const   List<VkLayerProperties> &       GetLayerProperties();
    const   List<VkExtensionProperties> &   GetExtensionProperties();
    const   bool                            CheckLayerSupport(const char *);

    Instance *CreateInstance(const UTF8String &,VKDebugOut *,CreateInstanceLayerInfo *);                            ///<创建一个Vulkan实例
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
