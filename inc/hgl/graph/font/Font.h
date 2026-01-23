#pragma once

#include<hgl/type/SortedSet.h>
#include<compare>

namespace hgl::graph
{
    constexpr size_t MAX_FONT_NAME_LENGTH=128;
        
    /**
    * 字体信息
    */
    struct Font
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

        std::strong_ordering operator<=>(const Font &other) const
        {
            if(auto cmp = width <=> other.width; cmp != 0)
                return cmp;

            if(auto cmp = height <=> other.height; cmp != 0)
                return cmp;

            if(auto cmp = bold <=> other.bold; cmp != 0)
                return cmp;
            
            if(auto cmp = italic <=> other.italic; cmp != 0)
                return cmp;
            
            if(auto cmp = anti <=> other.anti; cmp != 0)
                return cmp;

            return hgl::strcmp_ordering(name, other.name);
        }

        /**
         * 相等比较运算符
         * @param other 要比较的另一个 Font
         * @return 如果两个字体的所有属性都相同，返回 true；否则返回 false
         * 
         * 用法示例：
         * Font font1("Arial", 12, 12);
         * Font font2("Arial", 12, 12);
         * if(font1 == font2) { ... }
         */
        bool operator==(const Font &other) const
        {
            if(width != other.width)
                return false;
            
            if(height != other.height)
                return false;
            
            if(bold != other.bold)
                return false;
            
            if(italic != other.italic)
                return false;
            
            if(anti != other.anti)
                return false;
            
            return hgl::strcmp(name, other.name) == 0;
        }

        /**
         * 不相等比较运算符
         * @param other 要比较的另一个 Font
         * @return 如果两个字体的任何属性不同，返回 true；否则返回 false
         * 
         * 用法示例：
         * Font font1("Arial", 12, 12);
         * Font font2("Times", 14, 12);
         * if(font1 != font2) { ... }
         */
        bool operator!=(const Font &other) const
        {
            return !(*this == other);
        }

    };//struct Font

    using U32CharSet=SortedSet<u32char>;    ///<字符合集
}//namespace hgl::graph
