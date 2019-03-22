#include<hgl/graph/BufferObject.h>
namespace hgl
{
    namespace graph
    {
        BufferObject::~BufferObject()
        {
            SAFE_CLEAR(buffer_data);

            glDeleteBuffers(1,&buffer_index);
        }

        class BufferObjectBind:public BufferObject
        {
        public:

            using BufferObject::BufferObject;
            ~BufferObjectBind()=default;

            bool Submit(const void *data,GLsizeiptr size,GLenum up) override
            {
                if(!data||size<=0)return(false);

                user_pattern=up;
                glBindBuffer(buffer_type,buffer_index);
                glBufferData(buffer_type,size,data,user_pattern);

                return(true);
            }

            bool Change(const void *data,GLsizeiptr start,GLsizeiptr size) override
            {
                if(!data||start<0||size<=0)return(false);

                glBindBuffer(buffer_type,buffer_index);
                glBufferSubData(buffer_type,start,size,data);

                return(true);
            }
        };//class BufferObjectBind:public BufferObject

        class BufferObjectDSA:public BufferObject
        {
        public:

            using BufferObject::BufferObject;
            ~BufferObjectDSA()=default;

            bool Submit(const void *data,GLsizeiptr size,GLenum up) override
            {
                if(!data||size<=0)return(false);

                user_pattern=up;
                buffer_bytes=size;
                glNamedBufferData(buffer_index,size,data,user_pattern);

                return(true);
            }

            bool Change(const void *data,GLsizeiptr start,GLsizeiptr size) override
            {
                if(!data||start<0||size<=0)return(false);

                glNamedBufferSubData(buffer_index,start,size,data);

                return(true);
            }
        };//class BufferObjectDSA:public BufferObject

        BufferObject *CreateBuffer(GLenum type,GLenum user_pattern,BufferData *buf)
        {
            GLuint index;
            BufferObject *obj;

            if(GLEW_VERSION_4_5||GLEW_ARB_direct_state_access)
            {
                glCreateBuffers(1,&index);
                obj=new BufferObjectDSA(index,type);
            }
            else
            {
                glGenBuffers(1,&index);
                obj=new BufferObjectBind(index,type);
            }

            if(buf)
                obj->Submit(buf->GetData(),buf->GetTotalBytes(),user_pattern);

            return(obj);
        }
    }//namespace graph
}//namespace hgl
