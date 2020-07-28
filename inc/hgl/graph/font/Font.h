#ifndef HGL_GRAPH_FONT_INCLUDE
#define HGL_GRAPH_FONT_INCLUDE

#include<hgl/type/DataType.h>

namespace hgl
{
    namespace graph
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
            Font(const os_char *,int,int,bool,bool,bool=true);

            CompOperatorMemcmp(const Font &);	///<比较操作符重载
        };//struct Font
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_INCLUDE