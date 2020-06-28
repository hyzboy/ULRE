#include<hgl/graph/font/Font.h>

namespace hgl
{
	namespace graph
	{
		Font::Font()
		{
			memset(this,0,sizeof(Font));
		}
		
		Font::Font(const char *n,int w,int h,bool b,bool i,bool aa)
		{
			memset(this,0,sizeof(Font));

			strcpy(name,n);

			width=w;
			height=h;

			bold=b;
			italic=i;

			anti=aa;
		}
	}//namespace graph
}//namespace hgl
