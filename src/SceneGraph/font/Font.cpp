#include<hgl/graph/font/Font.h>
#include<hgl/type/StrChar.h>

namespace hgl
{
    namespace graph
    {
        Font::Font()
        {
            memset(this,0,sizeof(Font));
        }
        
        Font::Font(const os_char *n,int w,int h,bool b,bool i,bool aa)
        {
            memset(this,0,sizeof(Font));

            hgl::strcpy(name,MAX_FONT_NAME_LENGTH,n);

            width=w;
            height=h;

            bold=b;
            italic=i;

            anti=aa;
        }
    }//namespace graph
}//namespace hgl
