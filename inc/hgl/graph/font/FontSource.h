#ifndef HGL_GRAPH_FONT_SOURCE_INCLUDE
#define HGL_GRAPH_FONT_SOURCE_INCLUDE

#include<hgl/type/StrChar.h>
#include<hgl/type/Map.h>
#include<hgl/type/Set.h>
#include<hgl/graph/font/Font.h>

using namespace hgl;

namespace hgl
{
	namespace graph
	{
		/**
		* 字体位图数据
		*/
		struct FontBitmap
		{
			int count;		//使用次数

    		int x,y;		//图像显示偏移
			int w,h;		//图像尺寸

			int adv_x,adv_y;//字符尺寸

			uint8 *data;
		};//struct FontBitmap

		/**
		 * 文字位图数据源
		 */
		class FontSource
		{
		protected:

			Font fnt;

			MapObject<u32char,FontBitmap> chars_bitmap;												///<字符位图

			Set<void *> ref_object;

		protected:

			virtual bool MakeCharBitmap(FontBitmap *,u32char)=0;									///<产生字体数据
			virtual int  GetLineHeight()const=0;													///<取得行高

		public:

			FontSource(const Font &f){fnt=f;}
			virtual ~FontSource()=default;

			FontBitmap *GetCharBitmap(const u32char &);												///<取得字符位图数据

			void RefAcquire(void *);																///<引用请求
			void RefRelease(void *);																///<引用释放
			int  RefCount()const{return ref_object.GetCount();}										///<获取引用对象数量
		};//class FontSource
	}//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_SOURCE_INCLUDE