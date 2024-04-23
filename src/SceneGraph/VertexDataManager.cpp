﻿#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKVertexInputFormat.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKDevice.h>

namespace hgl
{
    namespace graph
    {
        VertexDataManager::VertexDataManager(GPUDevice *dev,const VIL *_vil)
        {
            device=dev;

            vil=_vil;
            vi_count=_vil->GetCount();
            vif_list=_vil->GetVIFList();     //来自于Material，不会被释放，所以指针有效

            vab_max_size=0;
            vab_cur_size=0;
            vab=hgl_zero_new<VAB *>(vi_count);

            ibo_cur_size=0;
            ibo=nullptr;
        }

        VertexDataManager::~VertexDataManager()
        {
            SAFE_CLEAR_OBJECT_ARRAY(vab,vi_count);
            SAFE_CLEAR(ibo);
        }

        /**
        * 初始化顶点数据管理器
        * @param vbo_size VBO大小
        * @param ibo_size IBO大小
        * @param index_type 索引类型
        */
        bool VertexDataManager::Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type)
        {
            if(vab[0]||ibo)     //已经初始化过了
                return(false);

            if(vbo_size<=0)
                return(false);

            vab_max_size=vbo_size;
            ibo_cur_size=ibo_size;

            vab_cur_size=0;
            ibo_cur_size=0;

            for(uint i=0;i<vi_count;i++)
            {
                vab[i]=device->CreateVAB(vif_list[i].format,vab_max_size);
                if(!vab[i])
                    return(false);
            }

            vbo_data_chain.Init(vab_max_size);

            if(ibo_size>0)
            {
                ibo=device->CreateIBO(index_type,ibo_size);
                if(!ibo)
                    return(false);

                ibo_data_chain.Init(ibo_size);
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
