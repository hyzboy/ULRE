#include<fstream>
#ifndef WIN32
#include<unistd.h>
#endif//

char *LoadFileToMemory(const char *filename,unsigned __int32 &file_length)
{
    std::ifstream fs;

    fs.open(filename,std::ios_base::binary);

    if(!fs.is_open())
        return(nullptr);

    fs.seekg(0,std::ios_base::end);
    file_length=fs.tellg();
    char *data=new char[file_length];

    fs.seekg(0,std::ios_base::beg);
    fs.read(data,file_length);

    fs.close();
    return data;
}