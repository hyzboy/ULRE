﻿#include<hgl/graph/BufferObject.h>
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
                    glNamedBufferStorage(index,size,nullptr,GL_MAP_WRITE_BIT);
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

                void BufferAlloc(GLenum type,GLuint index,GLsizeiptr size)
                {
                    glBindBuffer(type,index);
                    glBufferStorage(type,size,nullptr,GL_MAP_WRITE_BIT);
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
            static void(*BufferAlloc)(GLenum type,GLuint index,GLsizeiptr size);
            static void(*BufferData)(GLenum type,GLuint index,void *data,GLsizeiptr size,GLenum user_pattern);
            static void(*BufferSubData)(GLenum type,GLuint index,void *data,GLsizeiptr start,GLsizeiptr size);

            void InitBufferObjectAPI()
            {
                if(glCreateBuffers
                   ||glNamedBufferData
                   ||glNamedBufferSubData)       //dsa
                {
                    hgl::graph::gl::CreateBuffer=dsa::CreateBuffer;
                    hgl::graph::gl::BufferAlloc=dsa::BufferAlloc;
                    hgl::graph::gl::BufferData=dsa::BufferData;
                    hgl::graph::gl::BufferSubData=dsa::BufferSubData;
                }
                else
                    if(glNamedBufferDataEXT
                       ||glNamedBufferSubDataEXT)
                    {
                        hgl::graph::gl::CreateBuffer=bind::CreateBuffer;
                        hgl::graph::gl::BufferAlloc=dsa_ext::BufferAlloc;
                        hgl::graph::gl::BufferData=dsa_ext::BufferData;
                        hgl::graph::gl::BufferSubData=dsa_ext::BufferSubData;
                    }
                    else
                    {
                        hgl::graph::gl::CreateBuffer=bind::CreateBuffer;
                        hgl::graph::gl::BufferAlloc=bind::BufferAlloc;
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

        bool BufferObject::Create(GLsizeiptr size)
        {
            if(size<=0)return(false);

            gl::BufferAlloc(buffer_type,buffer_index,size);

            return true;
        }

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

            void *        data=buf_data->GetData();
            GLsizeiptr    size=buf_data->GetTotalBytes();

            if(!data||size<=0)return(false);

            buffer_data=buf_data;

            return Submit(data,size,up);
        }

        bool BufferObject::Change(void *data,GLsizeiptr start,GLsizeiptr size)
        {
            if(!data||start<0||size<=0)return(false);

            gl::BufferSubData(buffer_type,buffer_index,data,start,size);

            return(true);
        }
    }//namespace graph
}//namespace hgl