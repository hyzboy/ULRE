#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/vulkan/VKFormat.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            constexpr char SHADER_FILE_HEADER[]="Shader\x1A";
            constexpr uint SHADER_FILE_HEADER_BYTES=sizeof(SHADER_FILE_HEADER)-1;

            uint8 *LoadShaderStages(ShaderStageList &ss_list,uint8 *data)
            {
                const uint count=*data++;

                if(count<=0)
                    return(data);

                const uint32 total_bytes=*(uint32 *)data;
                data+=sizeof(uint32);

                int basetype;
                int vec_size;
                int str_len;

                ShaderStage *ss;

                for(uint i=0;i<count;i++)
                {
                    ss=new ShaderStage;

                    ss->location=*data++;
                    basetype=*data++;
                    vec_size=*data++;

                    ss->format=VK_NAMESPACE::GetVulkanFormat(basetype,vec_size);

                    str_len=*data++;
                    ss->name.SetString((char *)data,str_len);
                    data+=str_len;

                    ss_list.Add(ss);
                }

                return data;
            }

            uint8 *LoadShaderDescriptor(ShaderDescriptorList &sd_list,uint8 *data)
            {   
                const uint32 total_bytes=*(uint32 *)data;
                data+=sizeof(uint32);

                const uint count=*data++;

                uint str_len;

                for(uint i=0;i<count;i++)
                {
                    ShaderDescriptor *sd=new ShaderDescriptor;

                    sd->binding=*data++;
                    str_len=*data++;
                    sd->name.SetString((char *)data,str_len);
                    data+=str_len;

                    sd_list.Add(sd);
                }

                return data;
            }
        }//namespcae

        ShaderResource *LoadShaderResoruce(const OSString &filename)
        {
            int64 filesize;
            AutoDeleteArray<uint8> origin_filedata=(uint8 *)filesystem::LoadFileToMemory(filename,filesize);

            uint8 *filedata=origin_filedata;

            if(!filedata)return(nullptr);

            const uint8 *file_end=filedata+filesize;

            if(memcmp(filedata,SHADER_FILE_HEADER,SHADER_FILE_HEADER_BYTES))
                return(false);

            filedata+=SHADER_FILE_HEADER_BYTES;

            const uint8 ver=*filedata;
            ++filedata;

            const VkShaderStageFlagBits flag=(const VkShaderStageFlagBits)(*(uint32 *)filedata);
            filedata+=sizeof(uint32);

            const uint32 spv_size=*(uint32 *)filedata;
            filedata+=sizeof(uint32);

            const void *spv_data=filedata;
            filedata+=spv_size;

            ShaderResource *sr=new ShaderResource;

            filedata=LoadShaderStages(sr->GetInputStages(),filedata);
            filedata=LoadShaderStages(sr->GetOutputStages(),filedata);

            while(filedata<file_end)
            {
                uint32 desc_type=*(uint32 *)filedata;
                filedata+=sizeof(uint32);

                if(desc_type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER         )filedata=LoadShaderDescriptor(sr->GetUBO(),    filedata);else
                if(desc_type==VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )filedata=LoadShaderDescriptor(sr->GetSampler(),filedata);else
                {
                    delete sr;
                    return(nullptr);
                }
            }

            return sr;
        }
    }//namespace graph
}//namespace hgl
