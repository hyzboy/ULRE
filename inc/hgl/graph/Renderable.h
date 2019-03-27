#ifndef HGL_GRAPH_RENDERABLE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INCLUDE

#include<hgl/graph/VertexArray.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 可渲染对象
         */
        class Renderable
        {
        protected:

            uint primitive;                                                                         ///<绘制的图元类型
            VertexArray *vao;

//            Material *mtl;

        public:

            Renderable(uint prim,VertexArray *va=nullptr)
            {
                primitive=prim;
                vao=va;
            }

            const uint      GetPrimitive()const { return primitive; }                                ///<取得要绘制的图元类型

        public:

            uint            GetDrawCount();                                                         ///<取得可绘制的数据总数量
            bool            Draw();                                                                 ///<绘制
        };//class Renderable
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
