#ifndef HGL_GRAPH_FONT_SOURCE_INCLUDE
#define HGL_GRAPH_FONT_SOURCE_INCLUDE

#include<hgl/type/StrChar.h>
#include<hgl/type/Map.h>
#include<hgl/type/Set.h>
#include<hgl/graph/font/Font.h>
#include<hgl/type/UnicodeBlocks.h>

using namespace hgl;

namespace hgl
{
	namespace graph
	{
		struct CharMetricsInfo
		{
    		int x,y;		//图像显示偏移
			int w,h;		//图像尺寸

			int adv_x,adv_y;//字符尺寸
		};//struct CharMetricsInfo

		/**
		* 字体位图数据
		*/
		struct FontBitmap
		{
			int count;		//使用次数

			CharMetricsInfo metrics_info;

			uint8 *data;
		};//struct FontBitmap

		/**
		 * 文字位图数据源
		 */
		class FontSource
		{
		protected:

			Set<void *> ref_object;

		public:

			virtual ~FontSource()=default;

			virtual			FontBitmap *GetCharBitmap	(const u32char &)=0;						///<取得字符位图数据
			virtual const	bool		GetCharMetrics	(CharMetricsInfo &,const u32char &);		///<取得字符绘制信息
			virtual			int			GetCharHeight	()const=0;									///<取得字符高度

			void RefAcquire(void *);																///<引用请求
			void RefRelease(void *);																///<引用释放
			int  RefCount()const{return ref_object.GetCount();}										///<获取引用对象数量
		};//class FontSource

		/**
		 * 文字位图单一数据源
		 */
		class FontSourceSingle:public FontSource
		{
		protected:

			Font fnt;

			MapObject<u32char,FontBitmap> chars_bitmap;												///<字符位图

		protected:

			virtual bool MakeCharBitmap(FontBitmap *,u32char)=0;									///<产生字符位图数据

		public:

			FontSourceSingle(const Font &f){fnt=f;}
			virtual ~FontSourceSingle()=default;

							FontBitmap *GetCharBitmap	(const u32char &ch) override;				///<取得字符位图数据
					const	bool		GetCharMetrics	(CharMetricsInfo &,const u32char &);		///<取得字符绘制信息
			virtual			int			GetCharHeight	()const override{return fnt.height;}		///<取得字符高度
		};//class FontSourceSingle:public FontSource

		/**
		 * 文字位图多重数据源
		 */
        class FontSourceMulti:public FontSource
        {
            using FontSourcePointer=FontSource *;

			FontSource *default_source;
			Map<UnicodeBlock,FontSourcePointer> source_map;

			int max_char_height;

			void RefreshMaxCharHeight();

		protected:

			FontSource *GetFontSource(const u32char &ch);

        public:

			/**
			 * @param dfs 缺省字符数据源
			 */
            FontSourceMulti(FontSource *dfs);
            virtual ~FontSourceMulti();

            void Add(UnicodeBlock,FontSource *);
			void Remove(UnicodeBlock);
			void Remove(FontSource *);

		public:

					FontBitmap *GetCharBitmap	(const u32char &ch) override;
			const	bool		GetCharMetrics	(CharMetricsInfo &,const u32char &);				///<取得字符绘制信息
					int			GetCharHeight	()const override;									///<取得字符高度
        };//class FontSourceMulti:public FontSource
	}//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_SOURCE_INCLUDE
