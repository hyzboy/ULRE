#ifndef HGL_GRAPH_VERTEX_ARRAY_INCLUDE
#define HGL_GRAPH_VERTEX_ARRAY_INCLUDE

#include<hgl/type/List.h>
#include<hgl/graph/BufferObject.h>
#include<hgl/graph/PixelCompoment.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 顶点阵列数据
         */
        class VertexArray
        {
        protected:

            GLuint vao;

            List<ArrayBuffer *> vbo_list;                                                           ///<顶点数据缓冲区

            ElementBuffer *element_buffer;

            ArrayBuffer *position_buffer;
            int position_compoment;                                                                 ///<位置属性格式

        public:

            VertexArray();
            ~VertexArray();

            static int      GetMaxVertexAttrib();

            const GLuint    GetVAO      ()const{return vao;}                                        ///<取得VAO对象

        public: //通用顶点缓冲区设置

            int             AddBuffer   (int shader_location,ArrayBuffer *);                        ///<设置顶点缓冲区对象
            ArrayBuffer *   GetBuffer   (int index){return GetObject(vbo_list,index);}              ///<取得顶点缓冲区对象
            bool            ClearBuffer (int index){return vbo_list.Delete(index);}                 ///<清除顶点缓冲区对象
            void            ClearBuffers(){ vbo_list.Clear();}                                      ///<清除所有顶点缓冲区对象

        public: //特殊缓冲区独立设置函数

            bool            SetElement  (ElementBuffer *eb);                                        ///<设置索引缓冲区对象
            bool            SetPosition (int shader_location,ArrayBuffer *vb);                      ///<设置位置缓冲区对象

            ElementBuffer * GetElement  (){return element_buffer;}                                  ///<获取索引缓冲区对象
            ArrayBuffer *   GetPosition (){return position_buffer;}                                 ///<获取位置缓冲区对象
        };//class VertexArray
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ARRAY_INCLUDE
