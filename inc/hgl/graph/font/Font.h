#pragma once

#include<hgl/type/SortedSet.h>

namespace hgl::graph
{
    constexpr size_t MAX_FONT_NAME_LENGTH=128;
        
    /**
    * 字体信息
    */
    struct Font:public Comparator<Font>
    {
        os_char name[MAX_FONT_NAME_LENGTH];	///<字体名称

        int width;							///<宽度
        int height;							///<高度

        bool bold;							///<加粗
        bool italic;						///<右斜

        bool anti;							///<反矩齿

    public:

        Font();
        Font(const os_char *,int,int,bool b=false,bool i=false,bool=true);
        Font(const os_char *n,int s):Font(n,0,s,false,false,true){}

        const int compare(const Font &other)const override
        {
            int off;

            off=width-other.width;
            if(off)return off;

            off=height-other.height;
            if(off)return off;

            if(bold     !=other.bold    )return bold    ?1:-1;
            if(italic   !=other.italic  )return italic  ?1:-1;
            if(anti     !=other.anti    )return anti    ?1:-1;

            off=hgl::strcmp(name,other.name);
            return off;
        }
    };//struct Font

    using U32CharSet=SortedSet<u32char>;    ///<字符合集
}//namespace hgl::graph
