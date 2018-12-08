#include<GLEWCore/glew.h>
#include<hgl/type/DataType.h>
#include<fstream>
#include<iostream>

namespace hgl
{
    namespace graph
    {
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
        }//namespace

        bool LoadTGATexture(const std::string &filename,uint &tex_id)
        {
            std::ifstream file;

            file.open(filename,std::ifstream::in|std::ifstream::binary);

            if(!file.is_open())
            {
                std::cerr<<"[ERROR] open file<"<<filename.c_str()<<"> failed."<<std::endl;
                return(false);
            }

            file.seekg(0,std::ifstream::end);
            const int file_length=file.tellg();

            char *data=new char[file_length];

            file.seekg(0,std::ifstream::beg);
            file.read(data,file_length);
            file.close();

            TGAHeader *header=(TGAHeader *)data;

            uint videomemory_format;
            uint source_format;
            uint line_size=0;

            if(header->image_type==2)
            {
                if(header->bit==24)
                {
                    videomemory_format=GL_RGB8;
                    source_format=GL_BGR;
                    line_size=header->width*3;
                }
                else if(header->bit==32)
                {
                    videomemory_format=GL_RGBA8;
                    source_format=GL_BGRA;
                    line_size=header->width*4;
                }
            }
            else if(header->image_type==3&&header->bit==8)
            {
                videomemory_format=GL_R8;
                source_format=GL_RED;
                line_size=header->width;
            }
            else
            {
                std::cerr<<"[ERROR] Image format error,filename: "<<filename.c_str()<<std::endl;
                delete[] data;
                return(false);
            }

            glCreateTextures(GL_TEXTURE_2D,1,&tex_id);

            glTextureStorage2D(tex_id,1,videomemory_format,header->width,header->height);

            TGAImageDesc img_desc;

            img_desc.image_desc=header->image_desc;

            if(img_desc.direction==1)
                glTextureSubImage2D(tex_id,0,0,0,header->width,header->height,source_format,GL_UNSIGNED_BYTE,data+sizeof(TGAHeader));
            else
            {
                char *new_img=new char[line_size*header->height];
                char *sp=data+sizeof(TGAHeader)+line_size*(header->height-1);
                char *tp=new_img;

                for(int i=0;i<header->height;i++)
                {
                    memcpy(tp,sp,line_size);
                    tp+=line_size;
                    sp-=line_size;
                }

                glTextureSubImage2D(tex_id,0,0,0,header->width,header->height,source_format,GL_UNSIGNED_BYTE,new_img);
                delete[] new_img;
            }

            glTextureParameteri(tex_id,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTextureParameteri(tex_id,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTextureParameteri(tex_id,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTextureParameteri(tex_id,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

            std::cout<<"load image file<"<<filename.c_str()<<">:<"<<header->width<<"x"<<header->height<<"> to texture ok"<<std::endl;

            delete[] data;
            return(true);
        }
    }//namespace graph
}//namespace hgl
