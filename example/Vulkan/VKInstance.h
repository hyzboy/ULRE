#ifndef HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_INSTANCE_INCLUDE

#include<hgl/type/BaseString.h>
#include<hgl/type/List.h>
#include"Window.h"
#include"RenderSurface.h"

VK_NAMESPACE_BEGIN
    class Instance
    {
        Window *win;

        List<const char *> ext_list;

        VkApplicationInfo app_info;
        VkInstanceCreateInfo inst_info;

        VkInstance inst;

        List<VkPhysicalDevice> physical_devices;

    private:

        UTF8String app_name;

    public:

        Instance(const UTF8String &,Window *);
        virtual ~Instance();

        virtual bool Init();

        const List<VkPhysicalDevice> & GetDeviceList()const{return physical_devices;}

		RenderSurface *CreateRenderSurface(int pd_index=0);
    };//class Instance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
