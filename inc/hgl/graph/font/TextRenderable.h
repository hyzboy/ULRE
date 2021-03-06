#ifndef HGL_GRAPH_TEXT_RENDERABLE_INCLUDE
#define HGL_GRAPH_TEXT_RENDERABLE_INCLUDE

#include<hgl/graph/VKRenderable.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 文本可渲染对象
         */
        class TextRenderable:public Renderable
        {
            GPUDevice *    device;
            Material *  mtl;

                    uint        max_count;                                      ///<缓冲区最大容量

            VAB *       vab_position;
            VAB *       vab_tex_coord;

        public:

            TextRenderable(GPUDevice *,Material *,uint mc=1024);
            virtual ~TextRenderable();

        public:

            void SetCharCount   (const uint);

            bool WriteVertex    (const float *fp);
            bool WriteTexCoord  (const float *fp);
        };//class TextRenderable:public Renderable
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_RENDERABLE_INCLUDE
