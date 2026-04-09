#pragma once

// VKBufferMap.h is transitively included via VKVertexAttribBuffer.h below.
// Direct include removed — new code should use VKBufferAccessor.h directly.
#include<hgl/vk/VKBufferAccessor.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKMemory.h>
#include<cassert>
#include<cstdio>

namespace hgl::graph{
class BufferManager;
/**
 * 可绘制原始图形创建器
 */
class GeometryCreater
{
protected:

    VulkanDevice *      device;
    BufferManager *     buffer_manager;
    VertexDataManager * vdm;

    const VIL *         vil;

protected:

    AnsiString      geometry_name;
    GeometryData *  geometry_data;

    uint32_t        vertices_number;  ///<顶点数量

    bool            has_index;        ///<是否有索引
    uint32_t        index_number;     ///<索引数量
    IndexType       index_type;       ///<索引类型
    IndexBuffer *   ibo;              ///<索引缓冲区

    BufferAllocPolicy buffer_policy=BufferAllocPolicy::GPUOnly;

protected:

    const int InitVAB(const VertexAttrib &attrib,const VkFormat format,const void *data);                                       ///<取得顶点属性索引

    IndexBuffer * GetIBO();                                                                                                 ///<取得索引缓冲区
    int32_t GetFirstIndex()const;                                                                                           ///<取得第一个索引

public:

    GeometryCreater(VulkanDevice *,const VIL *,BufferManager *bm=nullptr);
    GeometryCreater(VertexDataManager *);
    virtual ~GeometryCreater();

            /**
            * 初始化一个原始图形创建
            * @parama name              原始图形名称
            * @parama vertices_count    顶点数量
            * @parama index_count       索引数量
            * @parama it                索引类型(注：当使用VDM时，此值无效)
            */
            bool            Init(const AnsiString &name,
                                 const uint32_t vertices_count,
                                 const uint32_t index_count=0,IndexType it=IndexType::AUTO);

            void            SetBufferPolicy(const BufferAllocPolicy policy){buffer_policy=policy;}
            const BufferAllocPolicy GetBufferPolicy()const{return buffer_policy;}

            void            Clear();                                                                                        ///<清除创建器数据

public: //顶点缓冲区

            const   uint32_t        GetVertexCount  ()const{ return vertices_number; }                                                ///<取得顶点数量
            int32_t                 GetVertexOffset ()const;                                                                         ///<取得顶点偏移(单位:元素)

            VertexAttribBuffer *    GetVAB          (const VertexAttrib attrib,const VkFormat format=VK_FORMAT_UNDEFINED);               ///<获取VAB用于BufferAccessor

            bool                    WriteVAB        (const VertexAttrib attrib,const VkFormat format,const void *data);                    ///<直接写入顶点属性数据

            /**
             * 创建带偏移的 BufferAccessor（自动使用VDM子分配的正确范围）
             * @tparam BufferAccessorType BufferAccessor类型（如 BufferAccessor3f）
             * @param attrib 顶点属性
             * @return 已绑定到正确偏移/数量的 BufferAccessor
             */
            template<typename BufferAccessorType>
            BufferAccessorType GetBufferAccessor(const VertexAttrib attrib)
            {
                VAB *vab = GetVAB(attrib);

                if(vab)
                {
                    const VkFormat expected = BufferAccessorType::DataAccessT::GetVulkanFormat();
                    const VkFormat actual = vab->GetFormat();

                    if(expected != VK_FORMAT_UNDEFINED && actual != expected)
                    {
                        std::fprintf(stderr,
                            "[GeometryCreater] GetBufferAccessor format mismatch: attrib='%s' expected='%s' actual='%s'\n",
                            GetVertexAttribName(attrib),
                            GetVulkanFormatName(expected),
                            GetVulkanFormatName(actual));

#ifdef _DEBUG
                        assert(false && "GeometryCreater::GetBufferAccessor format mismatch");
#endif

                        return BufferAccessorType();
                    }
                }

                return BufferAccessorType(vab, GetVertexOffset(), GetVertexCount());
            }

public: //索引缓冲区

    const   bool            hasIndex()const{return vdm?has_index:index_number>0;}
    const   IndexType       GetIndexType()const{return index_type;}
    const   uint32_t        GetIndexCount()const{return index_number;}

            /**
             * 创建 IndexAccessor（自动使用正确的索引类型）
             * @tparam T 索引类型（uint8, uint16, uint32）
             * @return 已绑定的 BufferAccessor
             */
            template<typename T>
            BufferAccessor<RawDataAccess<T>> GetIndexAccessor()
            {
                IndexBuffer *ibo = GetIBO();
                if(!ibo)
                    return BufferAccessor<RawDataAccess<T>>();

                return BufferAccessor<RawDataAccess<T>>(ibo, GetFirstIndex(), index_number);
            }

            bool            WriteIBO(const void *data,const uint32_t count);

            template<typename T>
            bool            WriteIBO(const T *data){return WriteIBO(data,index_number);}

public: //创建可渲染对象

            Geometry *     Create();                                                                                       ///<创建一个可渲染对象，并清除创建器数据

            /**
             * 创建几何体并设置AABB包围体
             * @param min_bounds AABB最小点
             * @param max_bounds AABB最大点
             * @return 创建的几何体指针
             */
            Geometry *     CreateWithAABB(const math::Vector3f& min_bounds, const math::Vector3f& max_bounds);
};//class GeometryCreater

Geometry *CreateGeometry(         VulkanDevice *  device,
                            const   VIL *           vil,
                            const   AnsiString &    name,
                            const   uint32_t        vertex_count,
                            const   uint32_t        index_count = 0,
                                    IndexType       it          = IndexType::AUTO,
                                    BufferManager * bm          = nullptr,
                                    BufferAllocPolicy policy    = BufferAllocPolicy::GPUOnly);
}//namespace hgl::graph
