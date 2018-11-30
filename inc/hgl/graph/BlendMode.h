#ifndef HGL_GRAPH_BLEND_MODE_INCLUDE
#define HGL_GRAPH_BLEND_MODE_INCLUDE

namespace hgl
{
    namespace graph
    {
        /**
         * 混合模式数据结构
         */
        struct BlendMode                ///混合模式
        {
            struct
            {
                unsigned int src,dst;   ///<混合因子
                unsigned int func;      ///<混合方式
            }rgb,alpha;
        };//struct BlendMode
    //namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BLEND_MODE_INCLUDE
