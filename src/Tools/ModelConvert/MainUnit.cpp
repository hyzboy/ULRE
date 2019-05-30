#include<iostream>
#include"AssimpLoader.h"

using namespace hgl;
using namespace hgl::graph;
using namespace VK_NAMESPACE;

#ifdef _WIN32
int wmain(int argc,wchar_t **argv)
#else
int main(int argc,char **argv)
#endif//
{
    std::cout<<"Model Convert 1.01"<<std::endl<<std::endl;

    if(argc<2)
    {
        std::cout<<"Model Converter\n"
                 "\n"
                 "Example: ModelConvert <model filename>"<<std::endl;
        return 0;
    }

    #ifdef _WIN32
    std::wcout<<L"file: "<<argv[1]<<std::endl;
    #else
    std::cout<<"file: "<<argv[1]<<std::endl;
    #endif//

    AssimpLoader al;

    al.LoadFile(argv[1]);

    return(0);
}
