#ifndef HGL_GRAPH_FONT_INCLUDE
#define HGL_GRAPH_FONT_INCLUDE

#include<hgl/CompOperator.h>
#include<string.h>

namespace hgl
{
	namespace graph
	{
		/**
		* 字体信息
		*/
		struct Font
		{
			os_char name[128];					///<字体名称

			int width;							///<宽度
			int height;							///<高度

			bool bold;							///<加粗
			bool italic;						///<右斜

			bool anti;							///<反矩齿

		public:

			Font();
			Font(const char *,int,int,bool,bool,bool=true);

			CompOperatorMemcmp(const Font &);	///<比较操作符重载
		};//struct Font
	}//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_INCLUDE