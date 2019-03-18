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

            void *      data;
            void *      data_end;

            GLenum      buffer_type;                                                                ///<缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
            GLenum      user_pattern;                                                               ///<数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)

            GLenum      data_type;                                                                  ///<单个数据类型 (GL_BYTE,GL_UNSIGNED_SHORT,GL_FLOAT等)
            uint        data_bytes;                                                                 ///<单个数据字节数 (GL_BYTE为1,GL_UNSIGNED_SHORT为2GL_FLOAT为4等)  
            uint        data_comp;                                                                  ///<数据成员数 (1/2/3/4，如2D纹理坐标用2，3D坐标/法线用3)

            GLsizeiptr  data_count;                                                                 ///<数据数量
            GLsizeiptr  total_bytes;                                                                ///<数据总字节数

        public:

            VertexBufferObject(const GLuint index,
                               const GLenum &buf_type,
                               const GLenum &dsup,
                               const uint &dt,const uint &dbytes,const uint &dcm,
                               const GLsizeiptr &count)
            {
                buffer_index=index;
                buffer_type=buf_type;
                user_pattern=dsup;

                data_type=dt;
                data_bytes=dbytes;
                data_comp=dcm;

                data_count=count;
                total_bytes=data_bytes*data_comp*data_count;
            }

            virtual ~VertexBufferObject()=default;

        public:

            GLuint      GetBuffer       ()const             {return buffer_index;}                  ///<取得OpenGL缓冲区
            GLenum      GetBufferType   ()const             {return buffer_type;}                   ///<取得缓冲区类型
            GLenum      GetUserPattern  ()const             {return user_pattern;}                  ///<取得缓冲区使用方法

            GLenum      GetDataType     ()const             {return data_type;}                     ///<取得数据类型
            uint        GetComponent    ()const             {return data_comp;}                     ///<取数每一组数据中的数据数量
            uint        GetStride       ()const             {return data_comp*data_bytes;}          ///<取得每一组数据字节数

            GLsizeiptr  GetCount        ()const             {return data_count;}                    ///<取得数据数量

            GLsizeiptr  GetTotalBytes   ()const             {return total_bytes;}                   ///<取得数据总字节数
            void *      GetData         ()                  {return data;}                          ///<取得数据指针
            void *      GetData         (const uint pos)    {return ((char *)data)+data_bytes*pos;} ///<取得数据指针

        public: 

            virtual void Update()=0;                                                                ///<向缓冲区提交所有数据
            virtual void Change(const GLsizeiptr start,const GLsizeiptr size)=0;                    ///<向缓冲区提交部分数据
            virtual void Change(const GLsizeiptr start,const GLsizeiptr size,const void *)=0;       ///<向缓冲区提交部分数据
        };//class VertexBufferObject

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
