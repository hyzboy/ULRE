#include<hgl/graph/font/Font.h>
#include<hgl/type/StrChar.h>

namespace hgl::graph
{
    Font::Font():Comparator<Font>()
    {
        mem_zero(name);
        width=height=0;
        bold=italic=anti=false;
    }
        
    Font::Font(const os_char *n,int w,int h,bool b,bool i,bool aa):Comparator<Font>()
    {
        hgl::strcpy(name,MAX_FONT_NAME_LENGTH,n);

        width=w;
        height=h;

        bold=b;
        italic=i;

        anti=aa;
    }
}//namespace hgl::graph
