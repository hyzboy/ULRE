#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/filesystem/FileSystem.h>

VK_NAMESPACE_BEGIN
namespace
{
    #pragma pack(push,1)
    struct TGAHeader
    {
        uint8 id;
        uint8 color_map_type;
        uint8 image_type;               // 1 colormap image ,2 true-color,3 grayscale

        uint16 color_map_first;
        uint16 color_map_length;
        uint8 color_map_size;

        uint16 x_origin;
        uint16 y_origin;

        uint16 width;
        uint16 height;
        uint8 bit;

        uint8 image_desc;
    };

    union TGAImageDesc
    {
        //不要把此union放到上面的struct中，否则Visual C++会将此union编译成4字节。GCC无此问题
        uint8 image_desc;
        struct
        {
            uint alpha_depth:4;
            uint reserved:1;
            uint direction:1;       //0 lower-left,1 upper left
        };
    };
    #pragma pack(pop)

    void RGB8to565(uint8 *data,uint size)
    {
        uint8 *src=data;
        uint16 *target=(uint16 *)data;

        for(uint i=0;i<size;i++)
        {
            *target=(((src[0])<<8)&0xF800)
                   |(((src[1])<<3)&0x7E0)
                   | ((src[2])>>3);

            ++target;
            src+=3;
        }
    }
}//namespace

Texture2D *LoadTGATexture(const OSString &filename,Device *device)
{
    uint8 *data;
    const int64 file_length=filesystem::LoadFileToMemory(filename,(void **)&data);

    if(file_length<=0)
    {
        std::cerr<<"[ERROR] open file<"<<filename.c_str()<<"> failed."<<std::endl;
        return(nullptr);
    }

    TGAHeader *header=(TGAHeader *)data;

    VkFormat format;
    uint pixels_size=0;

    if(header->image_type==2)
    {
        if(header->bit==24)
        {
            RGB8to565(data+sizeof(TGAHeader),header->width*header->height);

            format=FMT_RGB565;
            pixels_size=header->width*header->height*2;
        }
        else if(header->bit==32)
        {
            format=FMT_RGBA8UN;
            pixels_size=header->width*header->height*4;
        }
    }
    else if(header->image_type==3&&header->bit==8)
    {
        format=FMT_R8UN;
        pixels_size=header->width*header->height;
    }
    else
    {
        std::cerr<<"[ERROR] Image format error,filename: "<<filename.c_str()<<std::endl;
        delete[] data;
        return(nullptr);
    }

    Texture2D *tex=device->CreateTexture2D(format,data+sizeof(TGAHeader),header->width,header->height,pixels_size);

    if(tex)
    {
        std::cout<<"load image file<"<<filename.c_str()<<">:<"<<header->width<<"x"<<header->height<<"> to texture ok"<<std::endl;
    }
    else
    {
        std::cout<<"load image file<"<<filename.c_str()<<">:<"<<header->width<<"x"<<header->height<<"> to texture failed."<<std::endl;
    }

    delete[] data;
    return(tex);
}

//GLuint CreateSamplerObject(GLint min_filter,GLint mag_filter,GLint clamp,const GLfloat *border_color)
//{
//    GLuint sampler_object;

//    glGenSamplers(1,&sampler_object);

//    glSamplerParameteri(sampler_object,GL_TEXTURE_MIN_FILTER,min_filter);
//    glSamplerParameteri(sampler_object,GL_TEXTURE_MAG_FILTER,mag_filter);
//    glSamplerParameteri(sampler_object,GL_TEXTURE_WRAP_S,clamp);
//    glSamplerParameteri(sampler_object,GL_TEXTURE_WRAP_T,clamp);

//    glSamplerParameterfv(sampler_object,GL_TEXTURE_BORDER_COLOR,border_color);

//    return(sampler_object);
//}
VK_NAMESPACE_END