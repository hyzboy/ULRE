#include<hgl/graph/BufferData.h>

namespace hgl
{
    namespace graph
    {
        class BufferDataSelfAlloc:public BufferData
        {
            friend BufferData *CreateBufferData(const GLsizeiptr &count);

        public:

            using BufferData::BufferData;
            ~BufferDataSelfAlloc()
            {
                delete[] buffer_data;
            }
        };//class BufferDataSelfAlloc:public BufferData

        BufferData *CreateBufferData(const GLsizeiptr &total_bytes)
        {
            if(total_bytes<=0)
                return(nullptr);

            char *data=new char[total_bytes];

            if(!data)
                return(nullptr);

            return(new BufferDataSelfAlloc(data,total_bytes));
        }

        BufferData *CreateBufferData(void *data,const GLsizeiptr &count)
        {
            if(!data)
                return(nullptr);

            if(count<=0)
                return(nullptr);

            return(new BufferData((char *)data,count));
        }
    }
    namespace graph
    {
        class VertexBufferDataSelfAlloc:public VertexBufferData
        {
            friend VertexBufferData *CreateVertexBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count);

        public:

            using VertexBufferData::VertexBufferData;
            ~VertexBufferDataSelfAlloc()
            {
                delete[] buffer_data;
            }
        };//class BufferDataSelfAlloc:public VertexBufferData

        VertexBufferData *CreateVertexBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count)
        {
            if(dbytes<=0||dcm<=0||dcm>=5||count<=0)
                return(nullptr);

            const uint total_bytes=dbytes*dcm*count;

            char *data=new char[total_bytes];

            if(!data)
                return(nullptr);

            return(new VertexBufferDataSelfAlloc(dt,dbytes,dcm,count,data));
        }

        VertexBufferData *CreateVertexBufferData(void *data,const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count)
        {
            if(!data)
                return(nullptr);

            if(dbytes<=0||dcm<=0||dcm>=5||count<=0)
                return(nullptr);

            return(new VertexBufferData(dt,dbytes,dcm,count,(char *)data));
        }
    }//namespace graph
}//namespace hgl
