#include<iostream>
#include"AssimpLoader.h"

using namespace hgl;
using namespace hgl::graph;
using namespace VK_NAMESPACE;

int os_main(int argc,os_char **argv)
{
    std::cout<<"Assimp Import 1.01"<<std::endl<<std::endl;

    if(argc<2)
    {
        std::cout<<"Assimp Import\n"
                 "\n"
                 "Example: AssimpImport <model filename>"<<std::endl;
        return 0;
    }

    os_out<<OS_TEXT("file: "<<argv[1]<<std::endl;

    AssimpLoader al;

    al.LoadFile(argv[1]);

    return(0);
}
