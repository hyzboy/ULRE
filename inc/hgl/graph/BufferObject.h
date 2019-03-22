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

            friend BufferObject *CreateBuffer(GLenum type,GLenum user_pattern,BufferData *buf);

            BufferObject(GLuint index,GLenum type)
            {
                buffer_index=index;
                buffer_type =type;

                user_pattern=0;
                buffer_bytes=0;
                buffer_data=nullptr;
            }

        public:

            virtual ~BufferObject();

        public:

                    GLuint      GetBufferIndex  ()const {return buffer_index;}                      ///<取得OpenGL缓冲区
                    GLenum      GetBufferType   ()const {return buffer_type;}                       ///<取得缓冲区类型
                    GLenum      GetUserPattern  ()const {return user_pattern;}                      ///<取得缓冲区使用方法

            const   BufferData *GetBufferData   ()const {return buffer_data;}                       ///<取得缓冲数区(这里返回const是为了不想让只有BufferObject的模块可以修改数据)

        public:

            virtual bool        Submit          (const void *,GLsizeiptr,GLenum user_pattern)=0;    ///<提交数据
                    bool        Submit          (const BufferData *buf_data,GLenum user_pattern)    ///<提交数据
                    {
                        if(!buf_data)return(false);
                        buffer_data=buf_data;

                        const void *        data=buf_data->GetData();
                        const GLsizeiptr    size=buf_data->GetTotalBytes();

                        if(!data||size<=0)return(false);

                        return Submit(data,size,user_pattern);
                    }
            virtual bool        Change          (const void *,GLsizeiptr,GLsizeiptr)=0;             ///<修改数据
        };//class BufferObject

        /**
         * 创建一个缓冲区
         * @param type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 缓冲区数据用法(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param buf 数据缓冲区
         */
        BufferObject *CreateBuffer(GLenum type,GLenum user_pattern=0,BufferData *buf=nullptr);

        /**
         * 创建一个数据对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param total_bytes 数据总字节数
         */
        inline BufferObject *CreateBuffer(  const GLenum &buf_type,
                                            const GLenum &user_pattern,
                                            const GLsizeiptr &total_bytes)
        {
            if(total_bytes<=0)return(nullptr);

            BufferObject *buf=CreateBuffer(buf_type);

            if(!buf)
                return(nullptr);

            if(buf->Create(data,total_bytes))
                return buf;

            delete buf;
            return(nullptr);
        }

        /**
         * 创建一个数据对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param total_bytes 数据总字节数
         * @param data 数据指针
         */
        inline BufferObject *CreateBuffer(  const GLenum &buf_type,
                                            const GLenum &user_pattern,
                                            const GLsizeiptr &total_bytes,void *data)
        {
            if(total_bytes<=0)return(nullptr);
            if(!data)return(nullptr);

            BufferObject *buf=CreateBuffer(buf_type);

            if(!buf)
                return(nullptr);

            if(buf->Submit(data,total_bytes,user_pattern))
                return buf;

            delete buf;
            return(nullptr);
        }
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_BUFFER_OBJECT_INCLUDE
