#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/LogInfo.h>

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
            *target=((src[2]<<8)&0xF800)
                   |((src[1]<<3)&0x7E0)
                   | (src[0]>>3);

            ++target;
            src+=3;
        }
    }

    void SwapRow(uint8 *data,uint line_size,uint height)
    {
        uint8 *top=data;
        uint8 *bottom=data+(height-1)*line_size;
        uint8 *tmp=new uint8[line_size];

        while(top<bottom)
        {
            memcpy(tmp,bottom,line_size);
            memcpy(bottom,top,line_size);
            memcpy(top,tmp,line_size);

            top+=line_size;
            bottom-=line_size;
        }

        delete[] tmp;
    }
}//namespace

Texture2D *LoadTGATexture(const OSString &filename,Device *device)
{
    uint8 *data;
    const int64 file_length=filesystem::LoadFileToMemory(filename,(void **)&data);

    if(file_length<=0)
    {
        LOG_ERROR(OS_TEXT("[ERROR] open file<")+filename+OS_TEXT("> failed."));
        return(nullptr);
    }

    TGAHeader *header=(TGAHeader *)data;
    TGAImageDesc image_desc;
    uint8 *pixel_data=data+sizeof(TGAHeader);

    image_desc.image_desc=header->image_desc;

    VkFormat format;
    uint line_size;

    if(header->image_type==2)
    {
        if(header->bit==24)
        {
            RGB8to565(pixel_data,header->width*header->height);

            format=FMT_RGB565;
            line_size=header->width*2;
        }
        else if(header->bit==32)
        {
            format=FMT_RGBA8UN;
            line_size=header->width*4;
        }
    }
    else if(header->image_type==3&&header->bit==8)
    {
        format=FMT_R8UN;
        line_size=header->width;
    }
    else
    {
        LOG_ERROR(OS_TEXT("[ERROR] Image format error,filename: ")+filename);
        delete[] data;
        return(nullptr);
    }

    if(image_desc.direction==0)
        SwapRow(pixel_data,line_size,header->height);

    Texture2D *tex=device->CreateTexture2D(format,pixel_data,header->width,header->height,line_size*header->height);

    if(tex)
    {
        LOG_INFO(OS_TEXT("load image file<")+filename+OS_TEXT(">:<")+OSString(header->width)+OS_TEXT("x")+OSString(header->height)+OS_TEXT("> to texture ok"));

        //下面代码用于测试修改纹理
        //device->ChangeTexture2D(tex,pixel_data,header->width/4,header->height/4,header->width/2,header->height/2,line_size*header->height/4);
    }
    else
    {
        LOG_ERROR(OS_TEXT("load image file<")+filename+OS_TEXT(">:<")+OSString(header->width)+OS_TEXT("x")+OSString(header->height)+OS_TEXT("> to texture failed."));
    }

    delete[] data;
    return(tex);
}
VK_NAMESPACE_END