#include"RenderSurface.h"

VK_NAMESPACE_BEGIN

Buffer *RenderSurface::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode)
{
    VkBufferCreateInfo buf_info={};
    buf_info.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.pNext=nullptr;
    buf_info.usage=buf_usage;
    buf_info.size=size;
    buf_info.queueFamilyIndexCount=0;
    buf_info.pQueueFamilyIndices=nullptr;
    buf_info.sharingMode=sharing_mode;
    buf_info.flags=0;

    VkBuffer buf;

    if(vkCreateBuffer(rsa->device,&buf_info,nullptr,&buf)!=VK_SUCCESS)
        return(nullptr);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(rsa->device,buf,&mem_reqs);

    VkMemoryAllocateInfo alloc_info={};
    alloc_info.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.pNext=NULL;
    alloc_info.memoryTypeIndex=0;
    alloc_info.allocationSize=mem_reqs.size;

    if(rsa->CheckMemoryType(mem_reqs.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&alloc_info.memoryTypeIndex))
    {
        VkDeviceMemory mem;

        if(vkAllocateMemory(rsa->device,&alloc_info,nullptr,&mem)==VK_SUCCESS)
        {
            if(vkBindBufferMemory(rsa->device,buf,mem,0)==VK_SUCCESS)
            {
                VkDescriptorBufferInfo buf_info;

                buf_info.buffer=buf;
                buf_info.offset=0;
                buf_info.range=size;

                return(new Buffer(rsa->device,buf,mem,buf_info));
            }

            vkFreeMemory(rsa->device,mem,nullptr);
        }
    }

    vkDestroyBuffer(rsa->device,buf,nullptr);
    return(nullptr);
}

CommandBuffer *RenderSurface::CreateCommandBuffer()
{
    if(!rsa->cmd_pool)
        return(nullptr);

    VkCommandBufferAllocateInfo cmd={};
    cmd.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd.pNext=nullptr;
    cmd.commandPool=rsa->cmd_pool;
    cmd.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount=1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(rsa->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(nullptr);

    return(new CommandBuffer(rsa->device,rsa->cmd_pool,cmd_buf));
}

VK_NAMESPACE_END
