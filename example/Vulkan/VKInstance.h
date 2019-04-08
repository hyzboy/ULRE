#ifndef HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
#define HGL_GRAPH_VULKAN_INSTANCE_INCLUDE

#include<hgl/type/BaseString.h>
#include<vulkan/vulkan.h>

#define VK_NAMESPACE_BEGIN  namespace hgl{namespace graph{namespace vulkan{
#define VK_NAMESPACE_END    }}}

VK_NAMESPACE_BEGIN
    class Instance
    {
        VkApplicationInfo app_info;
        VkInstanceCreateInfo inst_info;

        VkInstance inst;

    private:

        UTF8String app_name;

    public:

        Instance(const UTF8String &an);
        virtual ~Instance();

        virtual bool Init();
    };//class Instance
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_INSTANCE_INCLUDE
