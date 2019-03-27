#include<hgl/graph/BufferObject.h>
namespace hgl
{
    namespace graph
    {
        namespace gl
        {
            namespace dsa
            {
                GLuint CreateBuffer()
                {
                    GLuint index;

                    glCreateBuffers(1,&index);

                    return index;
                }

                void BufferAlloc(GLenum type,GLuint index,GLsizeiptr size)
                {
                    glNamedBufferStorage(type,size,nullptr,GL_MAP_WRITE_BIT);
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
                void BufferAlloc(GLenum type,GLuint index,GLsizeiptr size)
                {
                    glNamedBufferStorageEXT(type,size,nullptr,GL_MAP_WRITE_BIT_EXT);
                }

                void BufferData(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern)
                {
                    glNamedBufferDataEXT(index,size,data,user_pattern);
                }

                void BufferSubData(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size)
                {
                    glNamedBufferSubDataEXT(index,start,size,data);
                }
            }//namespace dsa

            namespace bind_storage
            {

            }

            namespace bind
            {
                GLuint CreateBuffer()
                {
                    GLuint index;

                    glGenBuffers(1,&index);

                    return index;
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

            static GLuint(*CreateBuffer)();
            static void(*BufferData)(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern);
            static void(*BufferSubData)(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size);

            void InitBufferObjectAPI()
            {
                if(glCreateBuffers
                   ||glNamedBufferData
                   ||glNamedBufferSubData)       //dsa
                {
                    hgl::graph::gl::CreateBuffer=dsa::CreateBuffer;
                    hgl::graph::gl::BufferData=dsa::BufferData;
                    hgl::graph::gl::BufferSubData=dsa::BufferSubData;
                }
                else
                    if(glNamedBufferDataEXT
                       ||glNamedBufferSubDataEXT)
                    {
                        hgl::graph::gl::CreateBuffer=bind::CreateBuffer;
                        hgl::graph::gl::BufferData=dsa_ext::BufferData;
                        hgl::graph::gl::BufferSubData=dsa_ext::BufferSubData;
                    }
                    else
                    {
                        hgl::graph::gl::CreateBuffer=bind::CreateBuffer;
                        hgl::graph::gl::BufferData=bind::BufferData;
                        hgl::graph::gl::BufferSubData=bind::BufferSubData;
                    }
            }
        }//namespace gl

        BufferObject::BufferObject(GLenum type)
        {
            buffer_index=gl::CreateBuffer();
            buffer_type=type;

            user_pattern=0;
            buffer_bytes=0;
            buffer_data=nullptr;
        }

        BufferObject::~BufferObject()
        {
            SAFE_CLEAR(buffer_data);

            glDeleteBuffers(1,&buffer_index);
        }

        //bool BufferObject::Create(GLsizeiptr size,GLenum up)
        //{
        //    if(size<=0)return(false);

        //    return true;
        //}

        bool BufferObject::Submit(void *data,GLsizeiptr size,GLenum up)
        {
            if(!data||size<=0)return(false);

            user_pattern=up;
            buffer_bytes=size;
            gl::BufferData(buffer_type,buffer_index,data,size,user_pattern);

            return(true);
        }

        bool BufferObject::Submit(const BufferData *buf_data,GLenum up)
        {
            if(!buf_data)return(false);
            buffer_data=buf_data;

            void *        data=buf_data->GetData();
            GLsizeiptr    size=buf_data->GetTotalBytes();

            if(!data||size<=0)return(false);

            return Submit(data,size,up);
        }

        bool BufferObject::Change(void *data,GLsizeiptr start,GLsizeiptr size)
        {
            if(!data||start<0||size<=0)return(false);

            gl::BufferSubData(buffer_type,buffer_index,data,start,size);

            return(true);
        }
    }//namespace graph

    namespace graph
    {

        /**
         * 创建一个缓冲区对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param buf 数据缓冲区
         */
        template<typename BO,typename BD>
        BO *_CreateBufferObject(const GLenum &buf_type,const GLenum &user_pattern,BD *buf)
        {
            BO *obj=new BO(buf_type);

            if(buf)
                obj->Submit(buf,user_pattern);

            return(obj);
        }

        /**
         * 创建一个缓冲区对象
         * @param buf_type 缓冲区类型(GL_ARRAY_BUFFER,GL_ELEMENT_ARRAY_BUFFER等)
         * @param user_pattern 数据存储区使用模式(GL_STATIC_DRAW,GL_DYNAMIC_DRAW等)
         * @param total_bytes 数据总字节数
         */
        template<typename BO>
        BO *_CreateBufferObject(const GLenum &buf_type,
                                const GLenum &user_pattern,
                                const GLsizeiptr &total_bytes)
        {
            if(total_bytes<=0)return(nullptr);

            BO *buf=new BO(buf_type);

            //if(buf->Create(total_bytes,user_pattern))
            //    return buf;

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
        template<typename BO>
        inline BO *_CreateBufferObject( const GLenum &buf_type,
                                        const GLenum &user_pattern,
                                        const GLsizeiptr &total_bytes,void *data)
        {
            if(total_bytes<=0)return(nullptr);
            if(!data)return(nullptr);

            BO *buf=new BO(buf_type);

            //if(buf->Create(total_bytes,user_pattern))
            //    return buf;

            if(buf->Submit(data,total_bytes,user_pattern))
                return buf;

            delete buf;
            return(nullptr);
        }

        BufferObject *CreateBufferObject(const GLenum &type,const GLenum &up,BufferData *buf) { return _CreateBufferObject<BufferObject,BufferData>(type,up,buf); }
        BufferObject *CreateBufferObject(const GLenum &type,const GLenum &up,const GLsizeiptr &size) { return _CreateBufferObject<BufferObject>(type,up,size); }
        BufferObject *CreateBufferObject(const GLenum &type,const GLenum &up,const GLsizeiptr &size,void *data) { return _CreateBufferObject<BufferObject>(type,up,size,data); }
        VertexBufferObject *CreateVertexBufferObject(const GLenum &type,const GLenum &up,VertexBufferData *buf) { return _CreateBufferObject<VertexBufferObject,VertexBufferData>(type,up,buf); }
        VertexBufferObject *CreateVertexBufferObject(const GLenum &type,const GLenum &up,const GLsizeiptr &size) { return _CreateBufferObject<VertexBufferObject>(type,up,size); }
        VertexBufferObject *CreateVertexBufferObject(const GLenum &type,const GLenum &up,const GLsizeiptr &size,void *data) { return _CreateBufferObject<VertexBufferObject>(type,up,size,data); }
    }//namespace graph
}//namespace hgl
