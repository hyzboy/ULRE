#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKPhysicalDevice.h>
#include<hgl/graph/vulkan/VKRenderPass.h>

VK_NAMESPACE_BEGIN
//void CreateSubpassDependency(VkSubpassDependency *dependency)
//{   
//    dependency->srcSubpass      = VK_SUBPASS_EXTERNAL;
//    dependency->dstSubpass      = 0;
//    dependency->srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;            
//    dependency->srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
//    dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
//    dependency->dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
//    dependency->dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
//}

void CreateSubpassDependency(List<VkSubpassDependency> &subpass_dependency_list,const uint32_t count)
{
    if(count<=0)return;

    subpass_dependency_list.SetCount(count);
    
    VkSubpassDependency *dependency=subpass_dependency_list.GetData();

        dependency->srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependency->dstSubpass      = 0;
        dependency->srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;            
        dependency->srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
        dependency->dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    if(count==1)
    {        
        dependency->dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dependency->dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    }
    else
    {
        dependency->dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency->dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        ++dependency;

        for(uint32_t i=1;i<count;i++)
        {
                dependency->srcSubpass        = i-1;
                dependency->srcStageMask      = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if(i==count-1)
            {
                dependency->dstSubpass        = VK_SUBPASS_EXTERNAL;
                dependency->dstStageMask      = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                dependency->srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependency->dstAccessMask     = VK_ACCESS_MEMORY_READ_BIT;
            }
            else
            {
                dependency->dstSubpass        = i;
                dependency->dstStageMask      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                dependency->srcAccessMask     = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dependency->dstAccessMask     = VK_ACCESS_SHADER_READ_BIT;
            }

                dependency->dependencyFlags  = VK_DEPENDENCY_BY_REGION_BIT;

            ++dependency;
        }
    }
}

void CreateAttachmentReference(VkAttachmentReference *ref_list,uint start,uint count,VkImageLayout layout)
{
    VkAttachmentReference *ref=ref_list;

    for(uint i=start;i<start+count;i++)
    {
        ref->attachment =i;
        ref->layout     =layout;

        ++ref;
    }
}

bool CreateAttachmentDescription(List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    const uint color_count=color_format.GetCount();

    desc_list.SetCount(color_count+1);
    VkAttachmentDescription *desc=desc_list.GetData();

    for(uint i=0;i<color_count+1;i++)
    {
        desc->flags         = 0;
        desc->samples       = VK_SAMPLE_COUNT_1_BIT;
        desc->loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;      //LOAD_OP_CLEAR代表LOAD时清空内容
        desc->storeOp       = VK_ATTACHMENT_STORE_OP_STORE;     //STORE_OP_STROE代表SOTRE时储存内容
        desc->stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  //DONT CARE表示不在意
        desc->stencilStoreOp= VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;        //代表不关心初始布局

        ++desc;
    }

    desc=desc_list.GetData();
    const VkFormat *cf=color_format.GetData();
    for(uint i=0;i<color_count;i++)
    {
        desc->finalLayout      = color_final_layout;
        desc->format           = *cf;

        ++desc;
        ++cf;
    }

    desc->finalLayout  = depth_final_layout;
    desc->format       = depth_format;
    desc->storeOp      = VK_ATTACHMENT_STORE_OP_STORE;
    
    return(true);
}

bool CreateColorAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkImageLayout color_final_layout)
{
    //const VkFormat *cf=color_format_list.GetData();

    //for(int i=0;i<color_format_list.GetCount();i++)
    //{
    //    if(!attr->physical_device->IsColorAttachmentOptimal(*cf))
    //        return(false);

    //    ++cf;
    //}

    ref_list.SetCount(color_format.GetCount());
    VkAttachmentReference *ref=ref_list.GetData();

    desc_list.SetCount(color_format.GetCount());
    VkAttachmentDescription *desc=desc_list.GetData();

    for(int i=0;i<color_format.GetCount();i++)
    {    
        desc->flags             = 0;
        desc->samples           = VK_SAMPLE_COUNT_1_BIT;
        desc->loadOp            = VK_ATTACHMENT_LOAD_OP_CLEAR;      //LOAD_OP_CLEAR代表LOAD时清空内容
        desc->storeOp           = VK_ATTACHMENT_STORE_OP_STORE;     //STORE_OP_STROE代表SOTRE时储存内容
        desc->stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  //DONT CARE表示不在意
        desc->stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc->initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;        //代表不关心初始布局
        desc->finalLayout       = color_final_layout;
        ++desc;

        ref->attachment       = i;
        ref->layout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++ref;
    }

    return(true);
}

bool CreateDepthAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const VkFormat &depth_format,const VkImageLayout depth_final_layout)
{
    //if(!attr->physical_device->IsDepthAttachmentOptimal(depth_format))
    //    return(false);

    {
        ref_list.SetCount(1);
        VkAttachmentReference *ref=ref_list.GetData();

        ref->attachment=0;
        ref->layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    {
        desc_list.SetCount(1);
        VkAttachmentDescription *desc=desc_list.GetData();

        desc->flags             = 0;
        desc->samples           = VK_SAMPLE_COUNT_1_BIT;
        desc->loadOp            = VK_ATTACHMENT_LOAD_OP_CLEAR;      //LOAD_OP_CLEAR代表LOAD时清空内容
        desc->storeOp           = VK_ATTACHMENT_STORE_OP_DONT_CARE; //DONT CARE表示不在意
        desc->stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  
        desc->stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc->initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;        //代表不关心初始布局
        desc->finalLayout       = depth_final_layout;
    }

    return(true);
}

RenderPass *Device::CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                        const List<VkSubpassDescription> &subpass,
                                        const List<VkSubpassDependency> &dependency,
                                        const List<VkFormat> &color_format_list,
                                        const VkFormat depth_format,
                                        const VkImageLayout color_final_layout,
                                        const VkImageLayout depth_final_layout)
{
    for(const VkFormat cf:color_format_list)
    {
        if(!attr->physical_device->IsColorAttachmentOptimal(cf)
            &&!attr->physical_device->IsColorAttachmentLinear(cf))
            return(nullptr);
    }

    if(depth_format!=FMT_UNDEFINED)
    {
        if(!attr->physical_device->IsDepthAttachmentOptimal(depth_format)
         &&!attr->physical_device->IsDepthAttachmentLinear(depth_format))
            return(nullptr);
    }

    VkRenderPassCreateInfo rp_info;
    rp_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rp_info.pNext           = nullptr;
    rp_info.flags           = 0;
    rp_info.attachmentCount = desc_list.GetCount();
    rp_info.pAttachments    = desc_list.GetData();
    rp_info.subpassCount    = subpass.GetCount();
    rp_info.pSubpasses      = subpass.GetData();
    rp_info.dependencyCount = dependency.GetCount();
    rp_info.pDependencies   = dependency.GetData();

    VkRenderPass render_pass;

    if(vkCreateRenderPass(attr->device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(attr->device,render_pass,color_format_list,depth_format));
}

RenderPass *Device::CreateRenderPass(const List<VkFormat> &color_format_list,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    for(const VkFormat &fmt:color_format_list)
        if(!attr->physical_device->IsColorAttachmentOptimal(fmt))
            return(nullptr);

    List<VkAttachmentReference> color_ref_list;
    VkAttachmentReference depth_ref;
    List<VkAttachmentDescription> atta_desc_list;
    List<VkSubpassDescription> subpass_desc_list;
    List<VkSubpassDependency> subpass_dependency_list;

    color_ref_list.SetCount(color_format_list.GetCount());
    CreateColorAttachmentReference(color_ref_list.GetData(),0,color_format_list.GetCount());
    
    CreateAttachmentDescription(atta_desc_list,color_format_list,depth_format,color_final_layout,depth_final_layout);

    if(depth_format!=FMT_UNDEFINED)
    {
        if(!attr->physical_device->IsDepthAttachmentOptimal(depth_format))
            return(nullptr);
            
        CreateDepthAttachmentReference(&depth_ref,color_format_list.GetCount());
        subpass_desc_list.Add(SubpassDescription(color_ref_list.GetData(),color_ref_list.GetCount(),&depth_ref));
    }
    else
    {
        subpass_desc_list.Add(SubpassDescription(color_ref_list.GetData(),color_ref_list.GetCount()));
    }

    CreateSubpassDependency(subpass_dependency_list,2);

    return CreateRenderPass(atta_desc_list,subpass_desc_list,subpass_dependency_list,color_format_list,depth_format,color_final_layout,depth_final_layout);
}

RenderPass *Device::CreateRenderPass(VkFormat color_format,VkFormat depth_format,VkImageLayout color_final_layout,VkImageLayout depth_final_layout)
{
    List<VkFormat> color_format_list;

    color_format_list.Add(color_format);

    return CreateRenderPass(color_format_list,depth_format,color_final_layout,depth_final_layout);
}
VK_NAMESPACE_END
