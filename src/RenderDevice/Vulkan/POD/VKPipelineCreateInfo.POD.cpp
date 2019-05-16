#include<hgl/graph/vulkan/VKPipeline.h>
#include<hgl/type/BaseString.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/io/DataOutputStream.h>
#include<hgl/io/MemoryInputStream.h>
#include<hgl/io/DataInputStream.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;

VK_NAMESPACE_BEGIN
bool PipelineCreater::SaveToStream(io::DataOutputStream *dos)
{
    if(!dos)return(false);

    dos->WriteUint16(1);     //file ver

    dos->Write(&pipelineInfo,sizeof(VkGraphicsPipelineCreateInfo));

    dos->Write(pipelineInfo.pInputAssemblyState,    sizeof(VkPipelineInputAssemblyStateCreateInfo));
    dos->Write(pipelineInfo.pTessellationState,     sizeof(VkPipelineTessellationStateCreateInfo));
    dos->Write(pipelineInfo.pRasterizationState,    sizeof(VkPipelineRasterizationStateCreateInfo));

    dos->Write(pipelineInfo.pMultisampleState,      sizeof(VkPipelineMultisampleStateCreateInfo));
    if(pipelineInfo.pMultisampleState->pSampleMask)
    {
        const uint count=(pipelineInfo.pMultisampleState->rasterizationSamples+31)/32;
        dos->WriteUint8(count);
        dos->WriteUint32(pipelineInfo.pMultisampleState->pSampleMask,count);
    }
    else
    {
        dos->WriteUint8(0);
    }

    dos->Write(pipelineInfo.pDepthStencilState,    sizeof(VkPipelineDepthStencilStateCreateInfo));

    dos->Write(pipelineInfo.pColorBlendState,sizeof(VkPipelineColorBlendStateCreateInfo));
    for(uint32_t i=0;i<pipelineInfo.pColorBlendState->attachmentCount;i++)
        dos->Write(pipelineInfo.pColorBlendState->pAttachments+i,sizeof(VkPipelineColorBlendAttachmentState));

    return(true);
}

bool PipelineCreater::LoadFromMemory(uchar *data,uint size)
{
    uint16 ver=*(uint16 *)data;

    if(ver!=1)
        return(false);

    data+=sizeof(uint16);
    size-=sizeof(uint16);

    if(size<sizeof(VkGraphicsPipelineCreateInfo))
        return(false);

    memcpy(&pipelineInfo,data,sizeof(VkGraphicsPipelineCreateInfo));
    data+=sizeof(VkGraphicsPipelineCreateInfo);
    size-=sizeof(VkGraphicsPipelineCreateInfo);

    if(size<sizeof(VkPipelineInputAssemblyStateCreateInfo))return(false);
    pipelineInfo.pInputAssemblyState=(VkPipelineInputAssemblyStateCreateInfo *)data;
    data+=sizeof(VkPipelineInputAssemblyStateCreateInfo);
    size-=sizeof(VkPipelineInputAssemblyStateCreateInfo);

    if(size<sizeof(VkPipelineTessellationStateCreateInfo))return(false);
    pipelineInfo.pTessellationState=(VkPipelineTessellationStateCreateInfo *)data;
    data+=sizeof(VkPipelineTessellationStateCreateInfo);
    size-=sizeof(VkPipelineTessellationStateCreateInfo);

    if(size<sizeof(VkPipelineRasterizationStateCreateInfo))return(false);
    pipelineInfo.pRasterizationState=(VkPipelineRasterizationStateCreateInfo *)data;
    data+=sizeof(VkPipelineRasterizationStateCreateInfo);
    size-=sizeof(VkPipelineRasterizationStateCreateInfo);

    
    if(size<sizeof(VkPipelineMultisampleStateCreateInfo)+1)return(false);
    pipelineInfo.pMultisampleState=(VkPipelineMultisampleStateCreateInfo *)data;
    data+=sizeof(VkPipelineMultisampleStateCreateInfo);
    size-=sizeof(VkPipelineMultisampleStateCreateInfo);

    const uint8 count=*(uint8 *)data;

    if(count>0)
    {
        pipelineInfo.pMultisampleState->pSampleMask=(const VkSampleMask *)data;
        data+=count;
        size=count;
    }
    else
    {
        pipelineInfo.pMultisampleState->pSampleMask=nullptr;
    }

    if(size<sizeof(VkPipelineDepthStencilStateCreateInfo))return(false);
    pipelineInfo.pDepthStencilState=(VkPipelineDepthStencilStateCreateInfo *)data;
    data+=sizeof(VkPipelineDepthStencilStateCreateInfo);
    size-=sizeof(VkPipelineDepthStencilStateCreateInfo);
}
VK_NAMESPACE_END

bool SaveToFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc)
{
    if(filename.IsEmpty()||!pc)
        return(false);

    io::MemoryOutputStream mos;
    io::DataOutputStream *dos=new io::LEDataOutputStream(&mos);

    pc->SaveToStream(dos);

    delete dos;

    filesystem::SaveMemoryToFile(filename,mos.GetData(),mos.Tell());

    return(true);
}

bool LoadFromFile(const OSString &filename,VK_NAMESPACE::PipelineCreater *pc)
{
    if(filename.IsEmpty()||!pc)
        return(false);

    void *data;
    uint size=filesystem::LoadFileToMemory(filename,(void **)&data);

    bool result=pc->LoadFromMemory((uchar *)data,size);

    delete[] data;
    return result;
}