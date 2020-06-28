#include<hgl/graph/font/FontBitmapCache.h>

namespace hgl
{
    namespace graph
    {	
		FontBitmapCache::Bitmap *FontBitmapCache::GetCharBitmap(const u32char &ch)
		{
			if(!this)
				return(nullptr);

			if(hgl::isspace(ch))return(nullptr);	//不能显示的数据或是空格

			FontBitmapCache::Bitmap *bmp;
			
			if(chars_bitmap.Get(ch,bmp))
				return bmp;

			bmp=new FontBitmapCache::Bitmap;

			memset(bmp,0,sizeof(FontBitmapCache::Bitmap));

			if(!MakeCharBitmap(bmp,ch))
			{
				delete bmp;
				chars_bitmap.Add(ch,nullptr);
				return(nullptr);
			}
			else
			{
				chars_bitmap.Add(ch,bmp);
				return bmp;
			}
		}
    }//namespace graph
}//namespace hgl
