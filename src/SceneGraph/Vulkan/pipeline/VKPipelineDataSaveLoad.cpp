#include<hgl/graph/pipeline/VKPipelineData.h>
#include<hgl/type/String.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/io/DataOutputStream.h>
#include<hgl/io/MemoryInputStream.h>
#include<hgl/io/DataInputStream.h>
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;

VK_NAMESPACE_BEGIN
PipelineData::~PipelineData()
{
    if(file_data)
    {
        delete[] file_data;
        return;
    }

    SAFE_CLEAR_ARRAY(vertex_input_attribute_description);
    SAFE_CLEAR_ARRAY(vertex_input_binding_description);

    if(color_blend_attachments)
        hgl_free(color_blend_attachments);

    SAFE_CLEAR      (color_blend);

    SAFE_CLEAR      (depth_stencil);

    SAFE_CLEAR      (multi_sample);
    SAFE_CLEAR_ARRAY(sample_mask);

    SAFE_CLEAR      (rasterization);

    SAFE_CLEAR      (tessellation);
}

constexpr u8char PipelineFileHeader[]=u8"Pipeline\x1A";
constexpr size_t PipelineFileHeaderLength=sizeof(PipelineFileHeader)-1;

#define WRITE_AND_CHECK_SIZE(ptr,type)  if(dos->Write(ptr,sizeof(type))!=sizeof(type))return(false);

bool PipelineData::SaveToStream(io::DataOutputStream *dos)const
{
    if(!dos)return(false);

    if(dos->Write(PipelineFileHeader,PipelineFileHeaderLength)!=PipelineFileHeaderLength)return(false);
    if(!dos->WriteUint16(1))return(false);     //file ver

    if(!dos->WriteUint32(pipeline_info.stageCount))return(false);
    WRITE_AND_CHECK_SIZE(tessellation, VkPipelineTessellationStateCreateInfo   );
    WRITE_AND_CHECK_SIZE(rasterization,VkPipelineRasterizationStateCreateInfo  );
    WRITE_AND_CHECK_SIZE(multi_sample, VkPipelineMultisampleStateCreateInfo    );

    if(multi_sample->pSampleMask)
    {
        const uint count=(pipeline_info.pMultisampleState->rasterizationSamples+31)/32;
        if(!dos->WriteUint8(count))return(false);
        if(dos->WriteUint32(sample_mask,count)!=count)return(false);
    }
    else
    {
        if(!dos->WriteUint8(0))return(false);
    }

    WRITE_AND_CHECK_SIZE(depth_stencil,   VkPipelineDepthStencilStateCreateInfo);

    WRITE_AND_CHECK_SIZE(color_blend,     VkPipelineColorBlendStateCreateInfo);

    if(dos->WriteArrays<VkPipelineColorBlendAttachmentState>(color_blend_attachments,color_blend->attachmentCount)!=color_blend->attachmentCount)
        return(false);

    if(!dos->WriteFloat(alpha_test))return(false);

    return(true);
}

constexpr uint PIPELINE_FILE_MIN_LENGTH=PipelineFileHeaderLength+   //file header
                                        sizeof(uint16_t)+           //version
                                        sizeof(uint32_t)+           //stageCount
                                        sizeof(VkPipelineTessellationStateCreateInfo)+
                                        sizeof(VkPipelineRasterizationStateCreateInfo)+
                                        sizeof(VkPipelineMultisampleStateCreateInfo)+
                                        sizeof(uint8)+              //sample mask count
                                        sizeof(VkPipelineDepthStencilStateCreateInfo)+
                                        sizeof(VkPipelineColorBlendStateCreateInfo)+
                                        sizeof(float);              //alpha test

#define CHECK_SIZE_AND_EQUAL(val,type)  val=*(type *)data;  \
                                        data+=sizeof(type); \
                                        size-=sizeof(type);

#define CHECK_SIZE_AND_MAP(ptr,type)    ptr=(type *)data;\
                                        data+=sizeof(type); \
                                        size-=sizeof(type);

#define CHECK_SIZE_AND_MAP_ARRAY(ptr,type,count)    ptr=(type *)data;\
                                                    data+=sizeof(type)*count; \
                                                    size-=sizeof(type)*count;

bool PipelineData::LoadFromMemory(uchar *origin_data,uint size)
{
    uint8 *data=origin_data;

    if(size<PIPELINE_FILE_MIN_LENGTH)
        return(false);

    if(memcmp(data,PipelineFileHeader,PipelineFileHeaderLength)!=0)
        return(false);

    data+=PipelineFileHeaderLength;
    size-=PipelineFileHeaderLength;

    uint16_t ver;
    
    CHECK_SIZE_AND_EQUAL(ver,uint16_t)

    if(ver!=1)
        return(false);
    
    CHECK_SIZE_AND_EQUAL(pipeline_info.stageCount,uint32_t);
    CHECK_SIZE_AND_MAP(tessellation,VkPipelineTessellationStateCreateInfo);
    CHECK_SIZE_AND_MAP(rasterization,VkPipelineRasterizationStateCreateInfo);

    CHECK_SIZE_AND_MAP(multi_sample,VkPipelineMultisampleStateCreateInfo);

    const uint8 count=*(uint8 *)data;
    ++data;

    if(count>0)
    {
        CHECK_SIZE_AND_MAP_ARRAY(sample_mask,VkSampleMask,count);
        multi_sample->pSampleMask=sample_mask;
    }
    else
        multi_sample->pSampleMask=nullptr;

    CHECK_SIZE_AND_MAP(depth_stencil,VkPipelineDepthStencilStateCreateInfo);
    CHECK_SIZE_AND_MAP(color_blend,VkPipelineColorBlendStateCreateInfo);

    if(color_blend->attachmentCount>0)
    {
        CHECK_SIZE_AND_MAP_ARRAY(color_blend_attachments,VkPipelineColorBlendAttachmentState,color_blend->attachmentCount);

        color_blend->pAttachments=color_blend_attachments;
    }
    else
    {
        color_blend->pAttachments=nullptr;
        alpha_blend=false;
    }

    CHECK_SIZE_AND_EQUAL(alpha_test,float);
    
    pipeline_info.pInputAssemblyState=&input_assembly;
    pipeline_info.pTessellationState =tessellation;
    pipeline_info.pRasterizationState=rasterization;
    pipeline_info.pMultisampleState  =multi_sample;
    pipeline_info.pDepthStencilState =depth_stencil;
    pipeline_info.pColorBlendState   =color_blend;

    InitDynamicState();

    file_data=origin_data;
    return(true);
}

bool SaveToFile(const OSString &filename,PipelineData *pd)
{
    if(filename.IsEmpty()||!pd)
        return(false);

    io::MemoryOutputStream mos;
    io::DataOutputStream *dos=new io::LEDataOutputStream(&mos);

    pd->SaveToStream(dos);

    delete dos;

    filesystem::SaveMemoryToFile(filename,mos.GetData(),mos.Tell());

    return(true);
}

bool LoadFromFile(const OSString &filename,PipelineData *pd)
{
    if(filename.IsEmpty()||!pd)
        return(false);

    char *data;
    uint size=filesystem::LoadFileToMemory(filename,(void **)&data);

    bool result=pd->LoadFromMemory((uchar *)data,size);

    if(!result)
        delete[] data;

    return result;
}

PipelineData *LoadPipelineFromFile(const OSString &filename)
{
    if(filename.IsEmpty())
        return(nullptr);

    char *data;
    uint size=filesystem::LoadFileToMemory(filename,(void **)&data);

    if(!data)
        return(nullptr);

    PipelineData *pd=new PipelineData;

    bool result=pd->LoadFromMemory((uchar *)data,size);

    if(!result)
    {
        delete[] data;
        return(nullptr);
    }

    return pd;
}
VK_NAMESPACE_END
