#ifndef HGL_GRAPH_FONT_PRIMITIVE_INCLUDE
#define HGL_GRAPH_FONT_PRIMITIVE_INCLUDE

#include<hgl/graph/VKPrimitive.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 文本图元
         */
        class TextPrimitive:public Primitive
        {
            VulkanDevice * device;
            Material *  mtl;

            uint        max_count;                                      ///<缓冲区最大容量

            VAB *       vab_position;
            VAB *       vab_tex_coord;

        protected:

            friend class TextLayout;
            friend class TextRender;

            SortedSet<u32char> chars_sets;

            const SortedSet<u32char> &GetCharsSets()const{return chars_sets;}
            void SetCharsSets(const SortedSet<u32char> &sl){chars_sets=sl;}
            void ClearCharsSets(){chars_sets.Clear();}

        private:

            virtual ~TextPrimitive();

        public:

            TextPrimitive(VulkanDevice *,Material *,uint mc=1024);

        public:

            void SetCharCount   (const uint);

            bool WriteVertex    (const int16 *fp);
            bool WriteTexCoord  (const float *fp);
        };//class TextPrimitive:public Renderable
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_FONT_PRIMITIVE_INCLUDE
