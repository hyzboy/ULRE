#include<hgl/graph/vulkan/VK.h>
#include<iostream>

VK_NAMESPACE_USING;

int main(int,char **)
{
    #ifdef _DEBUG
        if(!CheckStrideBytesByFormat())
        {
            std::cerr<<"check stride bytes by format failed."<<std::endl;
            return(1);
        }
    #endif//_DEBUG

    for(uint32_t i=VK_FORMAT_BEGIN_RANGE;i<=VK_FORMAT_END_RANGE;i++)
    {
        const char *    name    =GetColorFormatName((VkFormat)i);
        const uint32_t  bytes   =GetStrideByFormat((VkFormat)i);

        if(name)
            std::cout<<"Format["<<i<<"]["<<name<<"] pixel is "<<bytes<<" bytes"<<std::endl;
    }

    return 0;
}