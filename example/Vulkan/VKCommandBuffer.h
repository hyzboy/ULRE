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

        bool Bind(VertexInput *vi)
        {
            if(!vi)
                return(false);

            const List<VkBuffer> &buf_list=vi->GetBufferList();

            if(buf_list.GetCount()<=0)
                return(false);

            constexpr VkDeviceSize zero_offsets[1]={0};

            vkCmdBindVertexBuffers(buf,0,buf_list.GetCount(),buf_list.GetData(),zero_offsets);

            return(true);
        }
    };//class CommandBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
