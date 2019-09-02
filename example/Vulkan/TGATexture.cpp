#include<hgl/graph/vulkan/VK.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
namespace
{
    namespace tga
    {
        enum class ImageType:uint8
        {
            ColorMap=1,
            TrueColor=2,
            Grayscale=3
        };

        enum class VerticalDirection:uint
        {
            BottomToTop=0,
            TopToBottom=1
        };

        #pragma pack(push,1)
        struct Header
        {
            uint8 id;
            uint8 color_map_type;
            ImageType image_type;               // 1 colormap image ,2 true-color,3 grayscale

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

        union ImageDesc
        {
            //不要把此union放到上面的struct中，否则Visual C++会将此union编译成4字节。GCC无此问题
            uint8 image_desc;
            struct
            {
                uint alpha_depth:4;
                uint horizontal_directon:1;     // 水平方向(不支持该参数)
                VerticalDirection vertical_direction:1;      // 0 bottom to top,1 top to bottom
            };
        };
        #pragma pack(pop)
    }//namespace tga

    void RGB8to565(uint16 *target,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *target=((src[2]<<8)&0xF800)
                   |((src[1]<<3)&0x7E0)
                   | (src[0]>>3);

            ++target;
            src+=3;
        }
    }

    void RGBtoRGBA(uint8 *tar,uint8 *src,uint size)
    {
        for(uint i=0;i<size;i++)
        {
            *tar++=*src++;
            *tar++=*src++;
            *tar++=*src++;
            *tar++=255;
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
    io::OpenFileInputStream fis(filename);

    if(!fis)
    {
        LOG_ERROR(OS_TEXT("[ERROR] open file<")+filename+OS_TEXT("> failed."));
        return(nullptr);
    }

    const int64 file_length=fis->GetSize();

    if(file_length<=sizeof(tga::Header))
    {
        LOG_ERROR(OS_TEXT("[ERROR] file<")+filename+OS_TEXT("> length < sizeof(tga::Header)."));
        return(nullptr);
    }

    tga::Header header;
    tga::ImageDesc image_desc;

    if(fis->Read(&header,sizeof(tga::Header))!=sizeof(tga::Header))
        return(false);
        
    const uint total_bytes=header.width*header.height*header.bit>>3;

    if(file_length<sizeof(tga::Header)+total_bytes)
    {
        LOG_ERROR(OS_TEXT("[ERROR] file<")+filename+OS_TEXT("> length error."));
        return(nullptr);
    }

    image_desc.image_desc=header.image_desc;

    VkFormat format=FMT_UNDEFINED;

    if(header.image_type==tga::ImageType::TrueColor)
    {
        if(header.bit==24)format=FMT_BGRA8UN;else
        if(header.bit==32)format=FMT_BGRA8UN;
    }
    else
    if(header.image_type==tga::ImageType::Grayscale)
    {
        if(header.bit== 8)format=FMT_R8UN;else
        if(header.bit==16)format=FMT_R16UN;
    }

    if(format==FMT_UNDEFINED)
    {
        LOG_ERROR(OS_TEXT("[ERROR] Image format error,filename: ")+filename);
        return(nullptr);
    }
    
    vulkan::Buffer *buf;

    if(header.bit==24)
    {
        uint8 *pixel_data=new uint8[total_bytes];
    
        fis->Read(pixel_data,total_bytes);

        if(image_desc.vertical_direction==tga::VerticalDirection::BottomToTop)
            SwapRow((uint8 *)pixel_data,header.width*header.bit/8,header.height);

        buf=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,header.width*header.height*4);
        RGBtoRGBA((uint8 *)buf->Map(),pixel_data,header.width*header.height);
        buf->Unmap();

        format=FMT_BGRA8UN;
    }
    else
    {
        vulkan::Buffer *buf=device->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,total_bytes);
        
        uint8 *pixel_data=(uint8 *)buf->Map();

        fis->Read(pixel_data,total_bytes);

        if(image_desc.vertical_direction==tga::VerticalDirection::BottomToTop)
            SwapRow((uint8 *)pixel_data,header.width*header.bit/8,header.height);

        buf->Unmap();
    }

    Texture2D *tex=device->CreateTexture2D(format,buf,header.width,header.height);

    delete buf;

    if(tex)
    {
        LOG_INFO(OS_TEXT("load image file<")+filename+OS_TEXT(">:<")+OSString(header.width)+OS_TEXT("x")+OSString(header.height)+OS_TEXT("> to texture ok"));

        //下面代码用于测试修改纹理
        //device->ChangeTexture2D(tex,pixel_data,header->width/4,header->height/4,header->width/2,header->height/2,line_size*header->height/4);
    }
    else
    {
        LOG_ERROR(OS_TEXT("load image file<")+filename+OS_TEXT(">:<")+OSString(header.width)+OS_TEXT("x")+OSString(header.height)+OS_TEXT("> to texture failed."));
    }

    return(tex);
}
VK_NAMESPACE_END
