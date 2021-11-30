#include<hgl/graph/VKDeviceRenderPassManage.h>
#include<hgl/graph/VKRenderPass.h>

VK_NAMESPACE_BEGIN
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

inline void CreateColorAttachmentReference(VkAttachmentReference *ref_list, uint start,uint count)  {CreateAttachmentReference(ref_list,    start,count,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);}
inline void CreateDepthAttachmentReference(VkAttachmentReference *depth_ref,uint index)             {CreateAttachmentReference(depth_ref,   index,1    ,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);}
inline void CreateInputAttachmentReference(VkAttachmentReference *ref_list, uint start,uint count)  {CreateAttachmentReference(ref_list,    start,count,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);}
    
bool CreateAttachmentDescription(List<VkAttachmentDescription> &desc_list,const RenderbufferInfo *rbi)
{
    const uint color_count=rbi->GetColorCount();

    const uint count=(rbi->HasDepthOrStencil())?color_count+1:color_count;

    desc_list.SetCount(count);

    VkAttachmentDescription *desc=desc_list.GetData();

    for(uint i=0;i<count;i++)
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
    const VkFormat *cf=rbi->GetColorFormat();
    for(uint i=0;i<color_count;i++)
    {
        desc->finalLayout      = rbi->GetColorLayout();
        desc->format           = *cf;

        ++desc;
        ++cf;
    }

    if(rbi->GetDepthFormat()!=PF_UNDEFINED)
    {
        desc->finalLayout  = rbi->GetDepthLayout();
        desc->format       = rbi->GetDepthFormat();
        desc->storeOp      = rbi->IsSwapchain()?VK_ATTACHMENT_STORE_OP_DONT_CARE:VK_ATTACHMENT_STORE_OP_STORE;
    }
    
    return(true);
}

bool CreateColorAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const List<VkFormat> &color_format,const VkImageLayout color_final_layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
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

bool CreateDepthAttachment( List<VkAttachmentReference> &ref_list,List<VkAttachmentDescription> &desc_list,const VkFormat &depth_format,const VkImageLayout depth_final_layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
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

DeviceRenderPassManage::DeviceRenderPassManage(VkDevice dev,VkPipelineCache pc)
{
    device=dev;
    pipeline_cache=pc;

    hash=util::CreateSHA1LEHash();
}

DeviceRenderPassManage::~DeviceRenderPassManage()
{
    SAFE_CLEAR(hash);
    
    const int count=RenderPassList.GetCount();

    auto *obj=RenderPassList.GetDataList();

    for(int i=0;i<count;i++)
    {
        delete (*obj)->right;

        ++obj;        
    }

    RenderPassList.Clear();
}

namespace
{
//    void HashRenderPass(RenderPassHASHCode *code,const VkRenderPassCreateInfo &rpci)
//    {
//        util::Hash *hash=util::CreateSHA1LEHash();
//
//        hash->Init();
//
////        hash->Write(rpci.attachmentCount);
//        hash->Write(rpci.pAttachments,rpci.attachmentCount);
////        hash->Write(rpci.subpassCount);
//        {
//            const VkSubpassDescription *sd=rpci.pSubpasses;
//
//            for(uint32_t i=0;i<rpci.subpassCount;i++)
//            {
//                hash->Write(sd->pipelineBindPoint);
//                hash->Write(sd->pInputAttachments,sd->inputAttachmentCount);
//                hash->Write(sd->pColorAttachments,sd->colorAttachmentCount);
//
//                if(sd->pResolveAttachments)
//                    hash->Write(*sd->pResolveAttachments);
//
//                if(sd->pDepthStencilAttachment)
//                    hash->Write(*sd->pDepthStencilAttachment);
//
//                hash->Write(sd->pPreserveAttachments,sd->preserveAttachmentCount);
//
//                ++sd;
//            }
//        }
//
//        hash->Write(rpci.pDependencies,rpci.dependencyCount);
//
//        hash->Final(code);
//
//        delete hash;
//    }

    void HashRenderPass(RenderPassHASHCode *code,const RenderbufferInfo *rbi,const uint8 subpass_count)
    {       
        util::Hash *hash=util::CreateSHA1LEHash();

        hash->Init();

        hash->Write<uint8>(subpass_count);

        for(const VkFormat &fmt:rbi->GetColorFormatList())
            hash->Write(fmt);

        hash->Final(code);
        delete hash;
    }
}

RenderPass *DeviceRenderPassManage::CreateRenderPass(   const List<VkAttachmentDescription> &desc_list,
                                                        const List<VkSubpassDescription> &subpass,
                                                        const List<VkSubpassDependency> &dependency,
                                                        const RenderbufferInfo *rbi)
{
    const VkFormat depth_format=rbi->GetDepthFormat();

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

    if(vkCreateRenderPass(device,&rp_info,nullptr,&render_pass)!=VK_SUCCESS)
        return(nullptr);

    return(new RenderPass(device,pipeline_cache,render_pass,rbi->GetColorFormatList(),depth_format));
}

RenderPass *DeviceRenderPassManage::AcquireRenderPass(const RenderbufferInfo *rbi,const uint subpass_count)
{
    RenderPassHASHCode hash;
    RenderPass *rp=nullptr;

    HashRenderPass(&hash,rbi,subpass_count);

    if(RenderPassList.Get(hash,rp))
        return rp;

    List<VkAttachmentReference> color_ref_list;
    VkAttachmentReference depth_ref;
    List<VkAttachmentDescription> atta_desc_list;
    List<VkSubpassDescription> subpass_desc_list;
    List<VkSubpassDependency> subpass_dependency_list;

    color_ref_list.SetCount(rbi->GetColorCount());
    CreateColorAttachmentReference(color_ref_list.GetData(),0,rbi->GetColorCount());
    
    CreateAttachmentDescription(atta_desc_list,rbi);

    if(rbi->HasDepthOrStencil())
    {           
        CreateDepthAttachmentReference(&depth_ref,rbi->GetColorCount());
        subpass_desc_list.Add(SubpassDescription(color_ref_list.GetData(),color_ref_list.GetCount(),&depth_ref));
    }
    else
    {
        subpass_desc_list.Add(SubpassDescription(color_ref_list.GetData(),color_ref_list.GetCount()));
    }

    CreateSubpassDependency(subpass_dependency_list,subpass_count);

    rp=CreateRenderPass(atta_desc_list,subpass_desc_list,subpass_dependency_list,rbi);

    RenderPassList.Add(hash,rp);

    return rp;
}
VK_NAMESPACE_END
