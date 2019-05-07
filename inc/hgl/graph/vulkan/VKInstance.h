#ifndef HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_INSTANCE_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include<hgl/platform/Window.h>
#include<hgl/graph/vulkan/VK.h>

VK_NAMESPACE_BEGIN
    class Instance
    {
        VkInstance inst;

        List<VkLayerProperties> layer_properties;
        List<VkExtensionProperties> extension_properties;

        VkDebugUtilsMessengerEXT debug_messenger;
        VkDebugReportCallbackEXT debug_report_callback;

        CharPointerList ext_list;

        ObjectList<PhysicalDevice> physical_devices;

    private:

        friend Instance *CreateInstance(const UTF8String &app_name);

        Instance(VkInstance,CharPointerList &);

    public:

        virtual ~Instance();

                operator VkInstance (){return inst;}

        const   List<VkLayerProperties> &   GetLayerProperties  ()const {return layer_properties;}
        const   bool                        CheckLayerSupport   (const UTF8String &)const;
        const   CharPointerList &           GetExtList          ()const {return ext_list;}
        const   ObjectList<PhysicalDevice> &GetDeviceList       ()const {return physical_devices;}
        const   PhysicalDevice *            GetDevice           (VkPhysicalDeviceType)const;
    };//class Instance

    Instance *CreateInstance(const UTF8String &);                                                   ///<创建一个Vulkan实例
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
