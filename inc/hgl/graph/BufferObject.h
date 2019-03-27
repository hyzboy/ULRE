#ifndef HGL_GRAPH_BUFFER_OBJECT_INCLUDE
#define HGL_GRAPH_BUFFER_OBJECT_INCLUDE

#include<hgl/graph/BufferData.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 显存数据缓冲区对象<br>
         * 负责对象与API的交接
         */
        class BufferObject
        {
        protected:

            GLuint      buffer_index;                                                               ///<缓冲区索引
            GLenum      buffer_type;                                                                ///<缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
            GLenum      user_pattern;                                                               ///<数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)

            GLsizeiptr  buffer_bytes;
            const BufferData *buffer_data;

        public:

            BufferObject(GLenum type);
            virtual ~BufferObject();

        public:

                    GLuint      GetBufferIndex  ()const {return buffer_index;}                      ///<取得OpenGL缓冲区
                    GLenum      GetBufferType   ()const {return buffer_type;}                       ///<取得缓冲区类型
                    GLenum      GetUserPattern  ()const {return user_pattern;}                      ///<取得缓冲区使用方法

            const   BufferData *GetBufferData   ()const {return buffer_data;}                       ///<取得缓冲数区(这里返回const是为了不想让只有BufferObject的模块可以修改数据)
            const   GLsizeiptr  GetBufferSize   ()const {return buffer_bytes;}                      ///<取得缓冲区总计字数

        public:

//                    bool        Create          (GLsizeiptr,GLenum up);                           ///<创建数据区
                    bool        Submit          (void *,GLsizeiptr,GLenum up);                      ///<提交数据
                    bool        Submit          (const BufferData *buf_data,GLenum up);             ///<提交数据
                    bool        Change          (void *,GLsizeiptr,GLsizeiptr);                     ///<修改数据
        };//class BufferObject

        BufferObject *CreateBufferObject(const GLenum &type,const GLenum &user_pattern=0,BufferData *buf=nullptr);                          ///<创建一个缓冲区对象
        BufferObject *CreateBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes);                  ///<创建一个缓冲区对象
        BufferObject *CreateBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes,void *data);       ///<创建一个缓冲区对象

        /**
         * 显存顶点属性数据缓冲区对象
         */
        class VertexBufferObject:public BufferObject
        {
            const VertexBufferData *vertex_buffer_data;

        public:

            using BufferObject::BufferObject;
            ~VertexBufferObject()=default;

            const   VertexBufferData *GetVertexBufferData()const { return vertex_buffer_data; }

        #define VBD_FUNC_COPY(type,name)    type Get##name()const{return vertex_buffer_data?vertex_buffer_data->Get##name():0;}

            VBD_FUNC_COPY(GLenum,DataType)
            VBD_FUNC_COPY(uint,Component)
            VBD_FUNC_COPY(uint,Stride)
            VBD_FUNC_COPY(GLsizeiptr,Count)

        #undef VBD_FUNC_COPY
        };//class VertexBufferObject:public BufferObject

        VertexBufferObject *CreateVertexBufferObject(const GLenum &type,const GLenum &user_pattern=0,VertexBufferData *buf=nullptr);                    ///<创建一个顶点缓冲区对象
        VertexBufferObject *CreateVertexBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes);                  ///<创建一个顶点缓冲区对象
        VertexBufferObject *CreateVertexBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes,void *data);       ///<创建一个顶点缓冲区对象

        using ElementBufferObject=VertexBufferObject;
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_OBJECT_INCLUDE
