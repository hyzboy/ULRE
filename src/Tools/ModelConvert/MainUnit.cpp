#include<iostream>
#include"AssimpLoader.h"
#include"FBXLoader.h"
#include<hgl/filesystem/FileSystem.h>

using namespace hgl;
using namespace hgl::graph;
using namespace VK_NAMESPACE;

bool IsFileExtension(const OSString& filename, const OSString& extension)
{
    size_t dot_pos = filename.find_last_of(OS_TEXT('.'));
    if(dot_pos == OSString::npos)
        return false;
        
    OSString file_ext = filename.substr(dot_pos);
    
    // 转换为小写进行比较
    for(auto& ch : file_ext)
        ch = tolower(ch);
        
    return file_ext == extension;
}

#ifdef _WIN32
int wmain(int argc,wchar_t **argv)
#else
int main(int argc,char **argv)
#endif//
{
    std::cout<<"Model Convert 1.02 (with FBX support)"<<std::endl<<std::endl;

    if(argc<2)
    {
        std::cout<<"Model Converter\n"
                 "\n"
                 "Supported formats:\n"
                 "  - FBX files (*.fbx) - via FBX SDK\n"
                 "  - Other formats - via Assimp\n"
                 "\n"
                 "Example: ModelConvert <model filename>"<<std::endl;
        return 0;
    }

    #ifdef _WIN32
    std::wcout<<L"file: "<<argv[1]<<std::endl;
    #else
    std::cout<<"file: "<<argv[1]<<std::endl;
    #endif//

    OSString filename = argv[1];
    
    // 检查文件扩展名来决定使用哪个加载器
    if(IsFileExtension(filename, OS_TEXT(".fbx")))
    {
        std::cout<<"Using FBX loader..."<<std::endl;
        FBXLoader fbx_loader;
        
        if(fbx_loader.LoadFile(filename))
        {
            std::cout<<"FBX file loaded successfully"<<std::endl;
            fbx_loader.PrintSceneInfo();
            
            // 保存到引擎格式（如果需要）
            // fbx_loader.SaveFile(filename + OS_TEXT(".scene"));
        }
        else
        {
            std::cerr<<"Failed to load FBX file"<<std::endl;
            return 1;
        }
    }
    else
    {
        std::cout<<"Using Assimp loader..."<<std::endl;
        AssimpLoader assimp_loader;
        
        if(!assimp_loader.LoadFile(filename))
        {
            std::cerr<<"Failed to load file with Assimp"<<std::endl;
            return 1;
        }
        
        std::cout<<"File loaded successfully with Assimp"<<std::endl;
    }

    return 0;
}
