#include<hgl/graph/shader/ShaderResource.h>
#include<hgl/filesystem/FileSystem.h>

namespace hgl
{
    namespace graph
    {
        ShaderResource *LoadShaderResoruce(const OSString &filename)
        {
            int64 filesize;
            uint8 *filedata=(uint8 *)filesystem::LoadFileToMemory(filename,filesize);

            if(!filedata)return(nullptr);


        }
    }//namespace graph
}//namespace hgl
