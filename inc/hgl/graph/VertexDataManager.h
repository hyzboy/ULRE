#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/type/DataChain.h>

namespace hgl
{
    namespace graph
    {
        class VertexDataManager
        {
            GPUDevice *device;

        protected:

            VIL *           vil;            ///<顶点输入格式列表
            uint            vi_count;       ///<顶点输入流数量
            const VIF *     vif_list;       ///<顶点输入格式列表

            VkDeviceSize    vbo_max_size;   ///<顶点缓冲区分配空间大小
            VkDeviceSize    vbo_cur_size;   ///<顶点缓冲区当前使用大小
            VBO **          vbo;            ///<顶点缓冲区列表

            VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
            IndexBuffer *   ibo;            ///<索引缓冲区

        protected:

            DataChain       vbo_data_chain; ///<数据链
            DataChain       ibo_data_chain; ///<数据链

        public:

            VertexDataManager(GPUDevice *dev,const VIL *_vil);
            ~VertexDataManager();

            const VIL *         GetVIL          ()const{return vil;}                                         ///<取得顶点输入格式列表

            const VkDeviceSize  GetVBOMaxCount  ()const{return vbo_max_size;}                                ///<取得顶点缓冲区分配的空间最大数量
            const VkDeviceSize  GetVBOCurCount  ()const{return vbo_cur_size;}                                ///<取得顶点缓冲区当前数量

            const IndexType     GetIBOType      ()const{return ibo?ibo->GetType():IndexType::ERR;}           ///<取得索引缓冲区类型
            const VkDeviceSize  GetIBOMaxCount  ()const{return ibo?ibo->GetCount():-1;}                      ///<取得索引缓冲区分配的空间最大数量
            const VkDeviceSize  GetIBOCurCount  ()const{return ibo?ibo_cur_size:-1;}                         ///<取得索引缓冲区当前数量

        public:

            bool Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type);
        };//class VertexDataManager
    }//namespace graph
}//namespace hgl
