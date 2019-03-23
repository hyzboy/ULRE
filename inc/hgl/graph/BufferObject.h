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

        protected:

            friend BufferObject *CreateBufferObject(GLenum,GLenum,BufferData *buf);
            friend BufferObject *CreateBufferObject(const GLenum &,const GLenum &,const GLsizeiptr &);
            friend BufferObject *CreateBufferObject(const GLenum &,const GLenum &,const GLsizeiptr &,void *);

            BufferObject(GLuint index,GLenum type);

        public:

            virtual ~BufferObject();

        public:

                    GLuint      GetBufferIndex  ()const {return buffer_index;}                      ///<取得OpenGL缓冲区
                    GLenum      GetBufferType   ()const {return buffer_type;}                       ///<取得缓冲区类型
                    GLenum      GetUserPattern  ()const {return user_pattern;}                      ///<取得缓冲区使用方法

            const   BufferData *GetBufferData   ()const {return buffer_data;}                       ///<取得缓冲数区(这里返回const是为了不想让只有BufferObject的模块可以修改数据)

        public:

                    bool        Create          (GLsizeiptr,GLenum user_pattern);                   ///<创建数据区
                    bool        Submit          (void *,GLsizeiptr,GLenum user_pattern);            ///<提交数据
                    bool        Submit          (const BufferData *buf_data,GLenum user_pattern);   ///<提交数据
                    bool        Change          (void *,GLsizeiptr,GLsizeiptr);                     ///<修改数据
        };//class BufferObject

        BufferObject *CreateBufferObject(GLenum type,GLenum user_pattern=0,BufferData *buf=nullptr);                                        ///<创建一个缓冲区对象
        BufferObject *CreateBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes);                  ///<创建一个缓冲区对象
        BufferObject *CreateBufferObject(const GLenum &buf_type,const GLenum &user_pattern,const GLsizeiptr &total_bytes,void *data);       ///<创建一个缓冲区对象
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_OBJECT_INCLUDE
