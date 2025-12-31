#include"FontDataSourceWin.h"
#include<hgl/color/sRGBConvert.h>

namespace hgl::graph
{
    namespace
    {
        void Convert8BitGrey(uint8 *dst,int dst_w,uint8 *src,int src_w,int src_h)
        {
            int pos;
            uint8 *sp=src,*p;
            uint8 *tp;

            const uint src_line_bytes=hgl_align<uint>(src_w,4);         //每行字节是32位对齐，这个已测试正常

            while(src_h--)
            {
                pos=src_w;
                p=sp;
                tp=dst;

                while(pos--)
                {
                    *tp=((*p>63)?255:(*p)<<2);                          ///<这个值是线性的，如果要在sRGB空间中使用，需要转换为sRGB空间

                    tp++;
                    p++;
                }

                sp+=src_line_bytes;
                dst+=dst_w;
            }
        }

        void ConvertBitmap(uint8 *dst,int dst_w,uint8 *src,int src_w,int src_h)
        {
            uint8 *sp=src,*p;
            uint8 *tp;
            uint8 bit;

            const uint src_line_bytes=hgl_align<uint>(src_w>>3,4);      //每行字节是32位对齐，这个未测试

            while(src_h--)
            {
                p=sp;
                tp=dst;

                bit=1<<7;

                for(int i=0;i<src_w;i++)
                {
                    *tp++=(*p&bit)?255:0;

                    if(bit==0x01)
                    {
                        bit=1<<7;
                        p++;
                    }
                    else bit>>=1;
                }

                sp+=src_line_bytes;
                dst+=dst_w;
            }//while(h--)
        }
    }//namespace

    WinBitmapFont::WinBitmapFont(const Font &f):FontBitmapDataSource(f)
    {
        hdc=CreateCompatibleDC(0);

        hfont=CreateFontW( -fnt.height,
                            fnt.width,
                            0,
                            0,
                            fnt.bold?FW_BOLD:FW_REGULAR,
                            fnt.italic,
                            false,
                            0,
                            DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            ANTIALIASED_QUALITY,
                            FF_DONTCARE,
                            fnt.name);

        SelectObject(hdc,hfont);

        if(!fnt.anti||fnt.height<=10)		//<=10象素强制用无抗矩齿字体
            ggo=GGO_BITMAP;
        else
            ggo=GGO_GRAY8_BITMAP;

        buffer.Reserve(fnt.width*fnt.height*4);
    }

    WinBitmapFont::~WinBitmapFont()
    {
        DeleteObject(hfont);
        DeleteDC(hdc);
    }
        
    bool WinBitmapFont::MakeCharBitmap(FontBitmap *bmp,u32char ch)
    {
        if(ch>0xFFFF)
            return(false);

        mem_zero(gm);
        mem_zero(mat);

        mat.eM11.value = 1;
        mat.eM22.value = 1;

        const int size=GetGlyphOutlineW(hdc,ch,ggo,&gm,0,0,&mat);

        if(size<=0)return(false);

        if(size>buffer.GetCount())
            buffer.Resize(size);

        GetGlyphOutlineW(hdc,ch,ggo,&gm,DWORD(buffer.GetCount()),buffer.data(),&mat);

        bmp->metrics_info.w		=hgl_align<uint>(gm.gmBlackBoxX,4);
        bmp->metrics_info.h		=hgl_align<uint>(gm.gmBlackBoxY,4);

        bmp->metrics_info.x		=gm.gmptGlyphOrigin.x;
        bmp->metrics_info.y		=gm.gmptGlyphOrigin.y;

        bmp->metrics_info.adv_x	=gm.gmCellIncX;
        bmp->metrics_info.adv_y	=gm.gmCellIncY;

        bmp->data=hgl_zero_new<uint8>(bmp->metrics_info.w*bmp->metrics_info.h);

        if(ggo==GGO_GRAY8_BITMAP)
            Convert8BitGrey	(bmp->data,bmp->metrics_info.w,buffer,gm.gmBlackBoxX,gm.gmBlackBoxY);
        else
            ConvertBitmap	(bmp->data,bmp->metrics_info.w,buffer,gm.gmBlackBoxX,gm.gmBlackBoxY);

        return(true);
    }

    FontDataSource *CreateFontSource(const Font &f)
    {
        return(new WinBitmapFont(f));
    }
}//namespace hgl::graph
