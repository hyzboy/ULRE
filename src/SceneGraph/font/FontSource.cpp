#include<hgl/graph/font/FontSource.h>

namespace hgl
{
    namespace graph
    {	
		FontBitmap *FontSource::GetCharBitmap(const u32char &ch)
		{
			if(!this)
				return(nullptr);

			if(hgl::isspace(ch))return(nullptr);	//不能显示的数据或是空格

			FontBitmap *bmp;
			
			if(chars_bitmap.Get(ch,bmp))
				return bmp;

			bmp=new FontBitmap;

			memset(bmp,0,sizeof(FontBitmap));

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

		void FontSource::RefAcquire(void *ptr)
		{
			if(!ptr)return;

			ref_object.Add(ptr);

			return;
		}

		void FontSource::RefRelease(void *ptr)
		{
			if(!ptr)return;

			ref_object.Delete(ptr);
		}
    }//namespace graph
}//namespace hgl
