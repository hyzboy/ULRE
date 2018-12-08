#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/VertexArray.h>
#include<GLEWCore/glew.h>
#include"VertexBufferControl.h"
#include<hgl/graph/RenderDriver.h>

namespace hgl
{
    namespace graph
    {
        VertexBufferControl *CreateVertexBufferControlDSA(uint);
        VertexBufferControl *CreateVertexBufferControlBind(uint);

        namespace
        {
            static VertexBufferControl *(*CreateVertexBufferControl)(uint)=nullptr;

            void InitVertexBufferAPI()
            {
                if(IsSupportDSA())
                    CreateVertexBufferControl=CreateVertexBufferControlDSA;
                else
                    CreateVertexBufferControl=CreateVertexBufferControlBind;
            }
        }//namespace

        void VertexBufferBase::SetDataSize(int size)
        {
            if (total_bytes == size)return;

            total_bytes = size;

            if (mem_data)
                mem_data = hgl_realloc(mem_data, size);
            else
                mem_data = hgl_malloc(size);

            mem_end = ((char *)mem_data) + size;
        }

        VertexBufferBase::VertexBufferBase(uint type,uint dt,uint dbyte,uint dcm,uint size,uint usage)
        {
            vb_type     =type;
            data_type   =dt;
            data_bytes  =dbyte;

            dc_num      =dcm;
            count       =size;
            total_bytes =dcm*size*dbyte;

            mem_data    =hgl_malloc(total_bytes);            //在很多情况下，hgl_malloc分配的内存是对齐的，这样有效率上的提升
            mem_end     =((char *)mem_data)+total_bytes;

            data_usage  =usage;

            if(!CreateVertexBufferControl)
                InitVertexBufferAPI();

            vbc=CreateVertexBufferControl(type);
        }

        VertexBufferBase::~VertexBufferBase()
        {
            hgl_free(mem_data);

            SAFE_CLEAR(vbc);
        }

        void VertexBufferBase::Update()
        {
            if(!vbc)return;

            vbc->Set(total_bytes,mem_data,data_usage);
        }

        void VertexBufferBase::Change(int start, int size, void *data)
        {
            if (!vbc)return;

            vbc->Change(start,size,data);
        }

        //         void VertexBufferBase::BindVertexBuffer()
        //         {
        //             if(!video_buffer_type)return;
        //
        //             glBindBuffer(video_buffer_type,video_buffer_index);
        //         }

        int VertexBufferBase::GetBufferIndex()const
        {
            return vbc?vbc->GetIndex():-1;
        }
    }//namespace graph
}//namespace hgl
