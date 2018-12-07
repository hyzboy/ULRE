#ifndef HGL_GRAPH_VERTEX_ARRAY_INCLUDE
#define HGL_GRAPH_VERTEX_ARRAY_INCLUDE

#include<hgl/type/List.h>
#include<hgl/graph/VertexBuffer.h>
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

            uint primitive;                                                                                             ///<绘制的图元类型

            ObjectList<VertexBufferBase> vertex_buffer_list;                                                            ///<顶点数据缓冲区

            int vertex_compoment;                                                                                       ///<顶点属性格式
            PixelCompoment color_compoment;                                                                             ///<颜色属性格式

            VertexBufferBase *element_buffer;
            VertexBufferBase *vertex_buffer;
            VertexBufferBase *color_buffer;

        public:

            VertexArray(uint prim,uint max_vertex_attrib);
            ~VertexArray();

            static int          GetMaxVertexAttrib();

            const uint          GetPrimitive        ()const{return primitive;}                                          ///<取得要绘制的图元类型
            const GLuint        GetVAO              ()const{return vao;}                                                ///<取得VAO对象

        public: //通用顶点缓冲区设置

            int                 AddVertexAttribBuffer     (VertexBufferBase *);                                         ///<设置顶点缓冲区数据
            VertexBufferBase *  GetVertexAttribBuffer     (int index){return vertex_buffer_list[index];}                ///<取得顶点缓冲区数据
            bool                ClearVertexAttribBuffer   (int index){return vertex_buffer_list.Delete(index);}         ///<清除顶点缓冲区数据
            void                ClearVertexAttribBuffers  (){vertex_buffer_list.Clear();}                               ///<清除所有顶点缓冲区数据

        public: //特殊缓冲区独立设置函数

            bool                SetElementBuffer    (VertexBufferBase *eb);                                             ///<设置索引缓冲区数据
            bool                SetVertexBuffer     (VertexBufferBase *vb);                                             ///<设置顶点缓冲区数据
            bool                SetColorBuffer      (VertexBufferBase *vb,PixelCompoment cf);                           ///<设置颜色缓冲区数据

            int                 GetVertexCompoment  ()const{return vertex_compoment;}                                   ///<取得顶点数据成分数量
            PixelCompoment      GetColorCompoment   ()const{return color_compoment;}                                    ///<取得顶点颜色格式

        public:

            int                 GetDrawCount    ();                                                                     ///<取得可绘制的数据总数量
            bool                Draw();                                                                                 ///<绘制
        };//class VertexArray
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_ARRAY_INCLUDE
