#include<vulkan/vulkan.h>
#include<hgl/type/BaseString.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/io/DataOutputStream.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;

namespace
{
    void Write(io::DataOutputStream *dos,const VkPipelineMultisampleStateCreateInfo *info)
    {
        dos->Write(info);

        if(info->pSampleMask)
            dos->WriteUint32(info->pSampleMask,(info->rasterizationSamples+31)/32);
    }

    void Write(io::DataOutputStream *dos,const VkPipelineColorBlendStateCreateInfo *info)
    {
        dos->Write(info,sizeof(VkPipelineColorBlendStateCreateInfo));

        for(uint32_t i=0;i<info->attachmentCount;i++)
            dos->Write(info->pAttachments+i,sizeof(VkPipelineColorBlendAttachmentState));
    }
}//namespace

bool SaveToFile(const OSString &filename,const VkGraphicsPipelineCreateInfo *info)
{
    if(filename.IsEmpty()||!info)
        return(false);

    io::MemoryOutputStream mos;
    io::DataOutputStream *dos=new io::LEDataOutputStream(&mos);

    dos->WriteUint16(1);     //file ver

    dos->Write(info,sizeof(VkGraphicsPipelineCreateInfo));

    dos->Write(info->pInputAssemblyState,   sizeof(VkPipelineInputAssemblyStateCreateInfo));
    dos->Write(info->pTessellationState,    sizeof(VkPipelineTessellationStateCreateInfo));
    dos->Write(info->pRasterizationState,   sizeof(VkPipelineRasterizationStateCreateInfo));
    Write(dos,info->pMultisampleState);
    dos->Write(info->pDepthStencilState,    sizeof(VkPipelineDepthStencilStateCreateInfo));
    Write(dos,info->pColorBlendState);

    delete dos;

    filesystem::SaveMemoryToFile(filename,mos.GetData(),mos.Tell());

    return(true);
}
