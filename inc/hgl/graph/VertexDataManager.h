#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/DataChain.h>

namespace hgl
{
    namespace graph
    {
        class VertexDataManager
        {
            GPUDevice *device;

        protected:

            uint            vi_count;       ///<顶点输入流数量
            const VIF *     vif_list;       ///<顶点输入格式列表

            VkDeviceSize    vbo_max_size;   ///<顶点缓冲区分配空间大小
            VkDeviceSize    vbo_cur_size;   ///<顶点缓冲区当前使用大小
            VBO **          vbo;            ///<顶点缓冲区列表

            VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
            IndexBuffer *   ibo;            ///<索引缓冲区

        protected:

            DataChain       data_chain;     ///<数据链

        public:

            VertexDataManager(GPUDevice *dev,const VIL *_vil)
            {
                device=dev;

                vi_count=_vil->GetCount();
                vif_list=_vil->GetVIFList();     //来自于Material，不会被释放，所以指针有效

                vbo_max_size=0;
                vbo_cur_size=0;
                vbo=hgl_zero_new<VBO *>(vi_count);

                ibo_cur_size=0;
                ibo=nullptr;
            }

            ~VertexDataManager()
            {
                SAFE_CLEAR_OBJECT_ARRAY(vbo,vi_count);
                SAFE_CLEAR(ibo);
            }

            const VkDeviceSize  GetVBOMaxCount  ()const{return vbo_max_size;}                                ///<取得顶点缓冲区分配的空间最大数量
            const VkDeviceSize  GetVBOCurCount  ()const{return vbo_cur_size;}                                ///<取得顶点缓冲区当前数量

            const IndexType     GetIBOType      ()const{return ibo?ibo->GetType():IndexType::ERR;}           ///<取得索引缓冲区类型
            const VkDeviceSize  GetIBOMaxCount  ()const{return ibo?ibo->GetCount():-1;}                      ///<取得索引缓冲区分配的空间最大数量
            const VkDeviceSize  GetIBOCurCount  ()const{return ibo?ibo_cur_size:-1;}                         ///<取得索引缓冲区当前数量

        public:

            bool Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type)
            {
                if(vbo[0]||ibo)     //已经初始化过了
                    return(false);

                if(vbo_size<=0)
                    return(false);

                vbo_max_size=vbo_size;
                ibo_cur_size=ibo_size;

                vbo_cur_size=0;
                ibo_cur_size=0;

                for(uint i=0;i<vi_count;i++)
                {
                    vbo[i]=device->CreateVBO(vif_list[i].format,vbo_max_size);
                    if(!vbo[i])
                        return(false);
                }

                if(ibo_size>0)
                {
                    ibo=device->CreateIBO(index_type,ibo_size);
                    if(!ibo)
                        return(false);
                }

                return(true);
            }
        };//class VertexDataManager
    }//namespace graph
}//namespace hgl
