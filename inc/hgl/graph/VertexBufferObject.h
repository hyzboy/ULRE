#ifndef HGL_GRAPH_VERTEX_BUFFER_OBJECT_INCLUDE
#define HGL_GRAPH_VERTEX_BUFFER_OBJECT_INCLUDE

#include<GLEWCore/glew.h>
#include<hgl/type/DataType.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 顶点缓冲区对象，对应OpenGL的VBO管理
         */
        class VertexBufferObject
        {
        protected:

            GLuint      buffer_index;

            GLenum      buffer_type;
            GLenum      data_type;
            uint        data_bytes;
            uint        data_comp;
            GLsizeiptr  data_count;
            GLsizeiptr  buffer_bytes;
            GLenum      user_pattern;                                                               ///<数据存储区使用模式

        public:

            VertexBufferObject(const GLuint index,
                               const GLenum &buf_type,
                               const uint &dt,const uint &dbytes,const uint &dcm,
                               const GLsizeiptr &count,
                               const GLenum &dsup)
            {
                buffer_index=index;
                buffer_type=buf_type;
                data_type=dt;
                data_bytes=dbytes;
                data_comp=dcm;
                data_count=count;
                buffer_bytes=data_bytes*data_comp*data_count;
                user_pattern=dsup;
            }

            virtual ~VertexBufferObject()=default;

            GLuint      GetBuffer       ()const{return buffer_index;}
            GLsizeiptr  GetBufferBytes  ()const{return buffer_bytes;}
        };//class VertexBufferObject

        /**
         * 创建一个VBO对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param data_type 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
         * @param data_bytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2GL_FLOAT为4等)
         * @param data_comp 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
         * @param size 数据数量
         * @param dsup 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         */
        VertexBufferObject *CreateVBO(  const GLenum &buf_type,
                                        const uint &data_type,const uint &data_bytes,const uint &data_comp,
                                        const GLsizeiptr &size,
                                        const GLenum &dsup);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_BUFFER_OBJECT_INCLUDE
