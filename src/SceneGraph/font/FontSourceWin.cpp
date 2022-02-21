#include"FontSourceWin.h"
#include<hgl/graph/ColorSpace.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            void Convert8BitGrey(uint8 *dst,int dst_w,uint8 *src,int src_w,int src_h,int src_line_bytes)
            {
                int pos;
                uint8 *sp=src,*p;
                uint8 *tp;

                while(src_h--)
                {
                    pos=src_w;
                    p=sp;
                    tp=dst;

                    while(pos--)
                    {
                        *tp=uint8(Clamp(Linear2sRGB(double(*p)/64.0f))*255.0f);

                        tp++;
                        p++;
                    }

                    sp+=src_line_bytes;
                    dst+=dst_w;
                }
            }

            void ConvertBitmap(uint8 *dst,int dst_w,uint8 *src,int src_w,int src_h,int src_line_bytes)
            {
                uint8 *sp=src,*p;
                uint8 *tp;
                uint8 bit;

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

        WinBitmapFont::WinBitmapFont(const Font &f):FontSourceSingle(f)
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

            buffer.Malloc(fnt.width*fnt.height*4);
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

            hgl_zero(gm);
            hgl_zero(mat);

            mat.eM11.value = 1;
            mat.eM22.value = 1;

            const int size=GetGlyphOutlineW(hdc,ch,ggo,&gm,0,0,&mat);

            if(size<=0)return(false);

            if(size>buffer.length())
                buffer.SetLength(size);

            GetGlyphOutlineW(hdc,ch,ggo,&gm,DWORD(buffer.length()),buffer.data(),&mat);

            bmp->metrics_info.w		=hgl_align_pow2<uint>(gm.gmBlackBoxX,4);
            bmp->metrics_info.h		=hgl_align_pow2<uint>(gm.gmBlackBoxY,4);

            bmp->metrics_info.x		=gm.gmptGlyphOrigin.x;
            bmp->metrics_info.y		=gm.gmptGlyphOrigin.y;

            bmp->metrics_info.adv_x	=gm.gmCellIncX;
            bmp->metrics_info.adv_y	=gm.gmCellIncY;

            bmp->data=hgl_zero_new<uint8>(bmp->metrics_info.w*bmp->metrics_info.h);

            if(ggo==GGO_GRAY8_BITMAP)
                Convert8BitGrey	(bmp->data,bmp->metrics_info.w,buffer,gm.gmBlackBoxX,gm.gmBlackBoxY,size/gm.gmBlackBoxY);
            else
                ConvertBitmap	(bmp->data,bmp->metrics_info.w,buffer,gm.gmBlackBoxX,gm.gmBlackBoxY,size/gm.gmBlackBoxY);

            return(true);
        }

        FontSource *CreateFontSource(const Font &f)
        {
            return(new WinBitmapFont(f));
        }
    }//namespace graph
}//namespace hgl
