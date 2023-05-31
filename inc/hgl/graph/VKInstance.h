#ifndef HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_INSTANCE_INCLUDE

#include<hgl/type/String.h>
#include<hgl/type/List.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/VKPhysicalDevice.h>
#include<hgl/graph/VKDebugOut.h>

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
            VK_BOOL1BIT(synchronization2)
            VK_BOOL1BIT(validation)
            VK_BOOL1BIT(profiles)
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

    class VulkanInstance
    {
        VkInstance inst;

        VKDebugOut *debug_out;

        ObjectList<GPUPhysicalDevice> physical_devices;

    private:
    
        PFN_vkGetDeviceProcAddr GetDeviceProcAddr;

    private:

        friend VulkanInstance *CreateInstance(const AnsiString &app_name,VKDebugOut *out=nullptr,CreateInstanceLayerInfo *cili=nullptr);

        VulkanInstance(VkInstance,VKDebugOut *);

    public:

        virtual ~VulkanInstance();

                operator VkInstance (){return inst;}

        const   ObjectList<GPUPhysicalDevice> &GetDeviceList       ()const {return physical_devices;}
        const   GPUPhysicalDevice *            GetDevice           (VkPhysicalDeviceType)const;
        
        template<typename T>
        T *GetInstanceProc(const char *name)
        {
            return reinterpret_cast<T>(vkGetInstanceProcAddr(inst,name));
        }

        template<typename T>
        T *GetDeviceProc(const char *name)
        {
            if(!GetDeviceProcAddr)return(nullptr);

            return reinterpret_cast<T>(GetDeviceProcAddr(name));
        }

        void DestroySurface(VkSurfaceKHR);
    };//class VulkanInstance
    
            void                            InitVulkanInstanceProperties();
    const   List<VkLayerProperties> &       GetInstanceLayerProperties();
    const   List<VkExtensionProperties> &   GetInstanceExtensionProperties();
    const   bool                            CheckInstanceLayerSupport(const AnsiString &);
    const   bool                            GetInstanceLayerVersion(const AnsiString &,uint32_t &spec,uint32_t &impl);
    const   bool                            CheckInstanceExtensionSupport(const AnsiString &);

    VulkanInstance *CreateInstance(const AnsiString &,VKDebugOut *,CreateInstanceLayerInfo *);                            ///<创建一个Vulkan实例
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
