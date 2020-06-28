#ifndef HGL_GRAPH_FONT_BITMAP_CACHE_INCLUDE
#define HGL_GRAPH_FONT_BITMAP_CACHE_INCLUDE

#include<hgl/type/StrChar.h>
#include<hgl/type/Map.h>
#include<hgl/graph/font/Font.h>

using namespace hgl;

namespace hgl
{
	namespace graph
	{
		/**
		 * 文本位图缓冲
		 */
		class FontBitmapCache
		{
			/**
			* 字体位图数据
			*/
			struct Bitmap
			{
				int count;		//使用次数

    			int x,y;		//图像显示偏移
				int w,h;		//图像尺寸

				int adv_x,adv_y;//字符尺寸

				uint8 *data;
			};//struct Bitmap

		protected:

			Font fnt;

			MapObject<u32char,FontBitmapCache::Bitmap> chars_bitmap;								///<字符位图

		protected:

			virtual bool MakeCharBitmap(FontBitmapCache::Bitmap *,u32char)=0;						///<产生字体数据
			virtual int  GetLineHeight()const=0;													///<取得行高

		public:

			FontBitmapCache(const Font &f){fnt=f;}
			virtual ~FontBitmapCache()=default;

			FontBitmapCache::Bitmap *GetCharBitmap(const u32char &);								///<取得字符位图数据
		};//class FontBitmapCache
	}//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_BITMAP_CACHE_INCLUDE