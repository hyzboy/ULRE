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

#define CHECK_SIZE_AND_COPY(ptr,type)   if(size<sizeof(type))return(false); \
                                        memcpy(&ptr,data,sizeof(type));  \
                                        data+=sizeof(type); \
                                        size-=sizeof(type);

bool PipelineCreater::LoadFromMemory(uchar *data,uint size)
{
    uint16 ver=*(uint16 *)data;

    if(ver!=1)
        return(false);

    data+=sizeof(uint16);
    size-=sizeof(uint16);

    CHECK_SIZE_AND_COPY(pipelineInfo,VkGraphicsPipelineCreateInfo);
    CHECK_SIZE_AND_COPY(inputAssembly,VkPipelineInputAssemblyStateCreateInfo);
    CHECK_SIZE_AND_COPY(tessellation,VkPipelineTessellationStateCreateInfo);
    CHECK_SIZE_AND_COPY(rasterizer,VkPipelineRasterizationStateCreateInfo);

    CHECK_SIZE_AND_COPY(multisampling,VkPipelineMultisampleStateCreateInfo);

    const uint8 count=*(uint8 *)data;
    ++data;

    if(count>0)
    {
        memcpy(sample_mask,data,count);
        multisampling.pSampleMask=sample_mask;
        data+=count;
        size=count;
    }
    else
    {
        multisampling.pSampleMask=nullptr;
    }

    CHECK_SIZE_AND_COPY(depthStencilState,VkPipelineDepthStencilStateCreateInfo);
    CHECK_SIZE_AND_COPY(colorBlending,VkPipelineColorBlendStateCreateInfo);

    if(colorBlending.attachmentCount>0)
    {
        if(size<colorBlending.attachmentCount*sizeof(VkPipelineColorBlendAttachmentState))
            return(false);

        colorBlendAttachments.SetCount(colorBlending.attachmentCount);
        memcpy(colorBlendAttachments.GetData(),data,colorBlending.attachmentCount*sizeof(VkPipelineColorBlendAttachmentState));

        colorBlending.pAttachments=colorBlendAttachments.GetData();
    }
    else
    {
        colorBlending.pAttachments=nullptr;
    }

    return(true);
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