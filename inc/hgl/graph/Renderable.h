#ifndef HGL_GRAPH_RENDERABLE_INCLUDE
#define HGL_GRAPH_RENDERABLE_INCLUDE

#include<hgl/graph/Shader.h>

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

            VertexArray *va;
            Material *mtl;
        };//class Renderable
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDERABLE_INCLUDE
