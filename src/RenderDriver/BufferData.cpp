#include<hgl/graph/BufferData.h>

namespace hgl
{
    namespace graph
    {
        class BufferDataSelfAlloc:public BufferData
        {
        public:

            using BufferData::BufferData;
            ~BufferDataSelfAlloc()
            {
                delete[] buffer_data;
            }
        };//class BufferDataSelfAlloc:public BufferData

        BufferData *CreateBufferData(const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count)
        {
            if(dbytes<=0||dcm<=0||dcm>=5||count<=0)
                return(nullptr);

            const uint total_bytes=dbytes*dcm*count;

            char *data=new char[total_bytes];

            if(!data)
                return(nullptr);

            return(new BufferDataSelfAlloc(dt,dbytes,dcm,count,data));
        }

        BufferData *CreateBufferData(void *data,const GLenum &dt,const uint &dbytes,const uint &dcm,const GLsizeiptr &count)
        {
            if(!data)
                return(nullptr);

            if(dbytes<=0||dcm<=0||dcm>=5||count<=0)
                return(nullptr);

            return(new BufferData(dt,dbytes,dcm,count,(char *)data));
        }
    }//namespace graph
}//namespace hgl
