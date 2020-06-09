#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/graph/vulkan/VKFormat.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            constexpr char      SHADER_FILE_HEADER[]    ="Shader\x1A";
            constexpr uint      SHADER_FILE_HEADER_BYTES=sizeof(SHADER_FILE_HEADER)-1;

            constexpr uint32    SHADER_FILE_MIN_SIZE    =SHADER_FILE_HEADER_BYTES
                                                        +1                         //version
                                                        +sizeof(uint32)            //shader flag
                                                        +sizeof(uint32)            //spv_size
                                                        +1                         //input states count
                                                        +1;                        //output states count

            const uint8 *LoadShaderStages(ShaderStageList &ss_list,const uint8 *data)
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

            const uint8 *LoadShaderDescriptor(ShaderDescriptorList &sd_list,const uint8 *data)
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

        ShaderResource::ShaderResource(const void *fd,const void *sd,const uint32 size)
        {
            data=fd;
            spv_data=sd;
            spv_size=size;            
        }

        ShaderResource::~ShaderResource()
        {
            delete[] data;
        }

        ShaderResource *LoadShaderResoruce(const OSString &filename)
        {
            int64 filesize;
            uint8 *origin_filedata=(uint8 *)filesystem::LoadFileToMemory(filename,filesize);

            if(!origin_filedata)return(nullptr);

            if(filesize<SHADER_FILE_MIN_SIZE
             ||memcmp(origin_filedata,SHADER_FILE_HEADER,SHADER_FILE_HEADER_BYTES))
            {
                delete[] origin_filedata;
                return(false);
            }

            const uint8 *filedata=origin_filedata;
            const uint8 *file_end=filedata+filesize;

            filedata+=SHADER_FILE_HEADER_BYTES;

            const uint8 ver=*filedata;
            ++filedata;

            const VkShaderStageFlagBits flag=(const VkShaderStageFlagBits)(*(uint32 *)filedata);
            filedata+=sizeof(uint32);

            const uint32 spv_size=*(uint32 *)filedata;
            filedata+=sizeof(uint32);

            ShaderResource *sr=new ShaderResource(origin_filedata,filedata,spv_size);

            filedata+=spv_size;

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
