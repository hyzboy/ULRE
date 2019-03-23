#include<hgl/graph/BufferObject.h>
namespace hgl
{
    namespace graph
    {
        namespace gl
        {
            namespace dsa
            {
                void CreateBuffer(GLenum type,GLuint *index)
                {
                    glCreateBuffers(1,index);
                }

                void BufferData(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern)
                {
                    glNamedBufferData(index,size,data,user_pattern);
                }

                void BufferSubData(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size)
                {
                    glNamedBufferSubData(index,start,size,data);
                }
            }//namespace dsa

            namespace dsa_ext
            {
                void BufferData(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern)
                {
                    glNamedBufferDataEXT(index,size,data,user_pattern);
                }

                void BufferSubData(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size)
                {
                    glNamedBufferSubDataEXT(index,start,size,data);
                }
            }//namespace dsa

            namespace bind
            {
                void CreateBuffer(GLenum type,GLuint *index)
                {
                    glGenBuffers(1,index);
                }

                void BufferData(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern)
                {
                    glBindBuffer(type,index);
                    glBufferData(type,size,data,user_pattern);
                }

                void BufferSubData(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size)
                {
                    glBindBuffer(type,index);
                    glBufferSubData(type,start,size,data);
                }
            }

            static void (*CreateBuffer)(GLenum type,GLuint *);
            static void (*BufferData)(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern);
            static void (*BufferSubData)(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size);

            void InitBufferObjectAPI()
            {
                if(glCreateBuffers
                 ||glNamedBufferData
                 ||glNamedBufferSubData)       //dsa
                {
                    hgl::graph::gl::CreateBuffer    =dsa::CreateBuffer;
                    hgl::graph::gl::BufferData      =dsa::BufferData;
                    hgl::graph::gl::BufferSubData   =dsa::BufferSubData;
                }
                else
                if(glNamedBufferDataEXT
                 ||glNamedBufferSubDataEXT)
                {
                    hgl::graph::gl::CreateBuffer    =bind::CreateBuffer;
                    hgl::graph::gl::BufferData      =dsa_ext::BufferData;
                    hgl::graph::gl::BufferSubData   =dsa_ext::BufferSubData;
                }
                else
                {
                    hgl::graph::gl::CreateBuffer    =bind::CreateBuffer;
                    hgl::graph::gl::BufferData      =bind::BufferData;
                    hgl::graph::gl::BufferSubData   =bind::BufferSubData;
                }
            }
        }//namespace gl

        BufferObject::BufferObject(GLuint index,GLenum type)
        {
            buffer_index=index;
            buffer_type =type;

            user_pattern=0;
            buffer_bytes=0;
            buffer_data=nullptr;
        }

        BufferObject::~BufferObject()
        {
            SAFE_CLEAR(buffer_data);

            glDeleteBuffers(1,&buffer_index);
        }

        bool BufferObject::Create(GLsizeiptr size,GLenum up)
        {
            return true;
        }

        bool BufferObject::Submit(void *data,GLsizeiptr size,GLenum up)
        {
            if(!data||size<=0)return(false);

            user_pattern=up;
            gl::BufferData(buffer_type,buffer_index,data,size,user_pattern);

            return(true);
        }

        bool BufferObject::Submit(const BufferData *buf_data,GLenum user_pattern)
        {
            if(!buf_data)return(false);
            buffer_data=buf_data;

            const void *        data=buf_data->GetData();
            const GLsizeiptr    size=buf_data->GetTotalBytes();

            if(!data||size<=0)return(false);

            return Submit(data,size,user_pattern);
        }

        bool BufferObject::Change(void *data,GLsizeiptr start,GLsizeiptr size)
        {
            if(!data||start<0||size<=0)return(false);

            gl::BufferSubData(buffer_type,buffer_index,data,start,size);

            return(true);
        }

        /**
         * 创建一个缓冲区对象
         * @param type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 缓冲区数据用法(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param buf 数据缓冲区
         */
        BufferObject *CreateBufferObject(GLenum type,GLenum user_pattern,BufferData *buf)
        {
            GLuint index;
            BufferObject *obj;

            gl::CreateBuffer(1,&index);

            obj=new BufferObject(index,type);

            if(buf)
                obj->Submit(buf->GetData(),buf->GetTotalBytes(),user_pattern);

            return(obj);
        }

        /**
         * 创建一个缓冲区对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param total_bytes 数据总字节数
         */
        BufferObject *CreateBufferObject( const GLenum &buf_type,
                                    const GLenum &user_pattern,
                                    const GLsizeiptr &total_bytes)
        {
            if(total_bytes<=0)return(nullptr);

            GLuint index;
            BufferObject *obj;

            gl::CreateBuffer(1,&index);

            BufferObject *buf=new BufferObject(index,buf_type);

            if(buf->Create(total_bytes,user_pattern))
                return buf;

            delete buf;
            return(nullptr);
        }

        /**
         * 创建一个缓冲区对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param total_bytes 数据总字节数
         * @param data 数据指针
         */
        inline BufferObject *CreateBufferObject(  const GLenum &buf_type,
                                            const GLenum &user_pattern,
                                            const GLsizeiptr &total_bytes,void *data)
        {
            if(total_bytes<=0)return(nullptr);
            if(!data)return(nullptr);

            GLuint index;
            BufferObject *obj;

            gl::CreateBuffer(1,&index);

            BufferObject *buf=new BufferObject(index,buf_type);

            if(buf->Create(total_bytes,user_pattern))
                return buf;

            if(buf->Submit(data,total_bytes,user_pattern))
                return buf;

            delete buf;
            return(nullptr);
        }
    }//namespace graph
}//namespace hgl
