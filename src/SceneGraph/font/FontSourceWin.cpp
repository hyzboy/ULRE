#include"FontSourceWin.h"

namespace hgl
{
	namespace graph
	{
		namespace
		{
			void Convert8BitGrey(uint8 *dst,uint8 *src,int w,int h,int line_bytes)
			{
				int pos;
				uint8 *sp=src,*p;

				while(h--)
				{
					pos=w;
					p=sp;

					while(pos--)
					{
						if(*p==64)*dst=255;
							else *dst=(*p)<<2;

						dst++;
						p++;
					}

					sp+=line_bytes;
				}
			}

			void ConvertBitmap(uint8 *dst,uint8 *src,int w,int h,int line_bytes)
			{
				uint8 *sp=src,*p;
				uint8 bit;

				while(h--)
				{
					p=sp;

					bit=1<<7;

					for(int i=0;i<w;i++)
					{
						*dst++=(*p&bit)?255:0;

						if(bit==0x01)
						{
							bit=1<<7;
							p++;
						}
						else bit>>=1;
					}

					sp+=line_bytes;				
				}//while(h--)
			}
		}//namespace

		WinBitmapFont::WinBitmapFont(const Font &f):FontSourceSingle(f)
		{
			hdc=CreateCompatibleDC(0);

			CharHeight=fnt.height;

			hfont=CreateFont(  -fnt.height,
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

			buffer_size=fnt.width*fnt.height*4;
			buffer=new uint8[buffer_size];
		}

		WinBitmapFont::~WinBitmapFont()
		{
			delete[] buffer;

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

			if(size>buffer_size)
			{
				delete[] buffer;

				buffer_size=size;
				buffer=new uint8[buffer_size];
			}

			GetGlyphOutlineW(hdc,ch,ggo,&gm,buffer_size,buffer,&mat);

			bmp->w=gm.gmBlackBoxX;
			bmp->h=gm.gmBlackBoxY;

			bmp->x=gm.gmptGlyphOrigin.x;
			bmp->y=fnt.height-gm.gmptGlyphOrigin.y-LineDistance;

			bmp->adv_x=gm.gmCellIncX;
			bmp->adv_y=gm.gmCellIncY;

			bmp->data=new uint8[bmp->w*bmp->h];

			if(ggo==GGO_GRAY8_BITMAP)
				Convert8BitGrey(bmp->data,buffer,gm.gmBlackBoxX,gm.gmBlackBoxY,size/bmp->h);
			else
				ConvertBitmap(bmp->data,buffer,gm.gmBlackBoxX,gm.gmBlackBoxY,size/bmp->h);

			return(true);
		}

		FontSource *CreateFontSource(const Font &f)
		{
			return(new WinBitmapFont(f));
		}
	}//namespace graph
}//namespace hgl