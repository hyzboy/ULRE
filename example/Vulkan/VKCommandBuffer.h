#ifndef HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE

#include"VK.h"
#include"VKVertexInput.h"

VK_NAMESPACE_BEGIN
    class CommandBuffer
    {
        VkDevice device;
        VkCommandPool pool;
        VkCommandBuffer buf;

    public:

        CommandBuffer(VkDevice dev,VkCommandPool cp,VkCommandBuffer cb){device=dev;pool=cp;buf=cb;}
        ~CommandBuffer();

        void Bind(VertexInput *vi)
        {
            auto &buf_list=vi->GetBufferList();

            constexpr VkDeviceSize offsets[1]={0};

            vkCmdBindVertexBuffers(buf,0,buf_list.GetCount(),buf_list.GetData(),offsets);
        }
    };//class CommandBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
