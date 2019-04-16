#include"RenderSurface.h"
#include<hgl/type/Pair.h>

VK_NAMESPACE_BEGIN

namespace
{
    bool CreateVulkanBuffer(VulkanBuffer &vb,const RenderSurfaceAttribute *rsa,VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode)
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

        if(vkCreateBuffer(rsa->device,&buf_info,nullptr,&vb.buffer)!=VK_SUCCESS)
            return(false);

        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(rsa->device,vb.buffer,&mem_reqs);

        VkMemoryAllocateInfo alloc_info={};
        alloc_info.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.pNext=nullptr;
        alloc_info.memoryTypeIndex=0;
        alloc_info.allocationSize=mem_reqs.size;

        if(rsa->CheckMemoryType(mem_reqs.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,&alloc_info.memoryTypeIndex))
        {
            VkDeviceMemory mem;

            if(vkAllocateMemory(rsa->device,&alloc_info,nullptr,&mem)==VK_SUCCESS)
            {
                if(vkBindBufferMemory(rsa->device,vb.buffer,mem,0)==VK_SUCCESS)
                {
                    vb.info.buffer=vb.buffer;
                    vb.info.offset=0;
                    vb.info.range=size;

                    return(true);
                }

                vkFreeMemory(rsa->device,mem,nullptr);
            }
        }

        vkDestroyBuffer(rsa->device,vb.buffer,nullptr);
        return(false);
    }
}//namespace

VertexBuffer *RenderSurface::CreateBuffer(VkBufferUsageFlags buf_usage,VkFormat format,uint32_t count,VkSharingMode sharing_mode)
{
    VulkanBuffer vb;

    const uint32_t stride=GetStrideByFormat(format);
    const VkDeviceSize size=stride*count;

    if(!CreateVulkanBuffer(vb,rsa,buf_usage,size,sharing_mode))
        return(nullptr);

    return(new VertexBuffer(rsa->device,vb,format,stride,count));
}

Buffer *RenderSurface::CreateBuffer(VkBufferUsageFlags buf_usage,VkDeviceSize size,VkSharingMode sharing_mode)
{
    VulkanBuffer vb;

    if(!CreateVulkanBuffer(vb,rsa,buf_usage,size,sharing_mode))
        return(nullptr);

    return(new Buffer(rsa->device,vb));
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

//DescriptorSet *RenderSurface::CreateDescSet(int count)
//{
//    VkDescriptorSetAllocateInfo alloc_info[1];
//    alloc_info[0].sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
//    alloc_info[0].pNext=nullptr;
//    alloc_info[0].descriptorPool=rsa->desc_pool;
//    alloc_info[0].descriptorSetCount=count;
//    alloc_info[0].pSetLayouts=desc_layout.data();
//
//    VkDescriptorSet desc_set;
//    
//    desc_set.resize(count);
//    res=vkAllocateDescriptorSets(info.device,alloc_info,info.desc_set.data());
//}

RenderPass *RenderSurface::CreateRenderPass()
{
    VkAttachmentDescription attachments[2];
    attachments[0].format=rsa->format;
    attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].flags=0;

    attachments[1].format=rsa->depth.format;
    attachments[1].samples=VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].flags=0;

    VkAttachmentReference color_reference={};
    color_reference.attachment=0;
    color_reference.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_reference={};
    depth_reference.attachment=1;
    depth_reference.layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass={};
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.flags=0;
    subpass.inputAttachmentCount=0;
    subpass.pInputAttachments=nullptr;
    subpass.colorAttachmentCount=1;
    subpass.pColorAttachments=&color_reference;
    subpass.pResolveAttachments=nullptr;
    subpass.pDepthStencilAttachment=&depth_reference;
    subpass.preserveAttachmentCount=0;
    subpass.pPreserveAttachments=nullptr;

    VkRenderPassCreateInfo rp_info={};
    rp_info.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext=nullptr;
    rp_info.attachmentCount=2;
    rp_info.pAttachments=attachments;
    rp_info.subpassCount=1;
    rp_info.pSubpasses=&subpass;
    rp_info.dependencyCount=0;
    rp_info.pDependencies=nullptr;

    VkRenderPass render_pass;

    if(vkCreateRenderPass(rsa->device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(rsa->device,render_pass));
}
VK_NAMESPACE_END
