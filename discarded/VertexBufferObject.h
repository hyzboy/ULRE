#ifndef HGL_GRAPH_VERTEX_BUFFER_OBJECT_INCLUDE
#define HGL_GRAPH_VERTEX_BUFFER_OBJECT_INCLUDE

#include<hgl/graph/BufferObject.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 创建一个VBO对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param dsup 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param data_type 单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
         * @param data_bytes 单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2GL_FLOAT为4等)
         * @param data_comp 数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)
         * @param size 数据数量
         */
        VertexBufferObject *CreateVBO(  const GLenum &buf_type,
                                        const GLenum &dsup,
                                        const uint &data_type,const uint &data_bytes,const uint &data_comp,
                                        const GLsizeiptr &size);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_VERTEX_BUFFER_OBJECT_INCLUDE
