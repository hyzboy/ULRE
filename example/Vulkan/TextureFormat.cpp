#include<hgl/graph/vulkan/VK.h>
#include<iostream>

VK_NAMESPACE_USING;

constexpr char *texture_compress_name[]=
{
    "NONE",
    "S3TC",
    "PVRTC",
    "ETC1",
    "ETC2",
    "EAC",
    "ATC",
    "ASTC",
    "YUV"
};

constexpr char *data_type_name[]
{
    "NONE",
    "UNORM",
    "SNORM",
    "USCALED",
    "SSCALED",
    "UINT",
    "SINT",
    "UFLOAT",
    "SFLOAT",
    "SRGB"
};//

int main(int,char **)
{
    #ifdef _DEBUG
        if(!CheckStrideBytesByFormat())
        {
            std::cerr<<"check stride bytes by format failed."<<std::endl;
            return(1);
        }
    #endif//_DEBUG

    uint32_t count;
    const VulkanFormat *vf=GetVulkanFormatList(count);

    if(!count)return(255);

    for(uint32_t i=0;i<count;i++)
    {
        std::cout<<i<<". ";
        
        std::cout<<" Format [ID:"<<vf->format<<"]["<<vf->name<<"] ";

        if(vf->compress_type!=TextureCompressType::NONE)
            std::cout<<"use "<<texture_compress_name[size_t(vf->compress_type)]<<" compress.";
        else
            std::cout<<vf->bytes<<" bytes/pixel.";

        if(vf->depth!=VulkanDataType::NONE)
            std::cout<<"[Depth:"<<data_type_name[size_t(vf->depth)]<<"]";
        
        if(vf->stencil!=VulkanDataType::NONE)
            std::cout<<"[Stencil:"<<data_type_name[size_t(vf->stencil)]<<"]";

        if((vf->depth==VulkanDataType::NONE)
         &&(vf->stencil==VulkanDataType::NONE))
            std::cout<<"[Color:"<<data_type_name[size_t(vf->color)]<<"]";
            
        std::cout<<std::endl;

        ++vf;
    }

    return 0;
}