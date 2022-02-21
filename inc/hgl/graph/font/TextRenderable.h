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
            GPUDevice * device;
            Material *  mtl;

            uint        max_count;                                      ///<缓冲区最大容量

            VBO *       vbo_position;
            VBO *       vbo_tex_coord;

        protected:

            friend class TextLayout;
            friend class TextRender;

            SortedSets<u32char> chars_sets;

            const SortedSets<u32char> &GetCharsSets()const{return chars_sets;}
            void SetCharsSets(const SortedSets<u32char> &sl){chars_sets=sl;}
            void ClearCharsSets(){chars_sets.Clear();}

        private:

            virtual ~TextRenderable();

        public:

            TextRenderable(GPUDevice *,Material *,uint mc=1024);

        public:

            void SetCharCount   (const uint);

            bool WriteVertex    (const int16 *fp);
            bool WriteTexCoord  (const float *fp);
        };//class TextRenderable:public Renderable
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_RENDERABLE_INCLUDE
