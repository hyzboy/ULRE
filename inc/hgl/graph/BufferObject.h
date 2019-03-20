#ifndef HGL_GRAPH_BUFFER_OBJECT_INCLUDE
#define HGL_GRAPH_BUFFER_OBJECT_INCLUDE

#include<GLEWCore/glew.h>
#include<hgl/type/DataType.h>
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
            GLuint      buffer_index;                                                               ///<缓冲区索引
            GLenum      buffer_type;                                                                ///<缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
            GLenum      user_pattern;                                                               ///<数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)

            GLsizeiptr  total_bytes;                                                                ///<数据总字节数
            void *local_data,*local_data_end;                                                       ///<本地数据内存指针
            bool self_alloc;                                                                        ///<是否自身分配

        protected:

            BufferObject(GLenum type)
            {
                buffer_index=
                buffer_type=type;
            }

            BufferObject(GLenum type,GLenum up)
            {
                buffer_type=type;
                user_pattern=up;
            }

            BufferObject(GLuint index,GLenum type,GLenum up)
            {
                buffer_index=index;
                buffer_type=type;
                user_pattern=up;
            }

            BufferObject(GLenum type,GLenum up,void *data,GLsizeiptr size,bool data_self_alloc)
            {
                buffer_index=index;
                buffer_type=type;
                user_pattern=up;

                total_bytes=size;

                local_data=data;
                local_data_end=((char *)data)+size;

                self_alloc=data_self_alloc;
            }

            virtual ~BufferObject()
            {
                if(self_alloc)
                    if(local_data)delete[] local_data;
            }

            GLuint      GetBuffer       ()const             {return buffer_index;}                  ///<取得OpenGL缓冲区
            GLenum      GetBufferType   ()const             {return buffer_type;}                   ///<取得缓冲区类型
            GLenum      GetUserPattern  ()const             {return user_pattern;}                  ///<取得缓冲区使用方法
            GLsizeiptr  GetTotalBytes   ()const             {return total_bytes;}                   ///<取得数据总字节数

            void *      GetData         ()                  {return data;}                          ///<取得数据指针
            void *      GetData         (const uint pos)    {return ((char *)data)+data_bytes*pos;} ///<取得数据指针
        };//class BufferObject

    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_OBJECT_INCLUDE
