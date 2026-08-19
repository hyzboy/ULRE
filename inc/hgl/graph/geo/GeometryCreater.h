#pragma once

// VKBufferMap.h is transitively included via VKVertexAttribBuffer.h below.
// Direct include removed — new code should use VKBufferAccessor.h directly.
#include<hgl/vk/VKBufferAccessor.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKMemory.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph{
class BufferManager;

/**
 * float → half 位模式（IEEE 754 半精度）——VB2hf（T=uint16）写入前必须显式转换
 */
inline uint16 FloatToHalf(float f)
{
    uint32_t x;
    memcpy(&x, &f, sizeof(x));
    const uint32_t sign = (x >> 16) & 0x8000u;
    const int32_t exp   = (int32_t)((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = x & 0x7FFFFFu;

    if(exp <= 0)
    {
        if(exp < -10)
            return (uint16)sign;
        mant |= 0x800000u;
        return (uint16)(sign | (mant >> (uint32_t)(14 - exp)));
    }
    if(exp >= 31)
        return (uint16)(sign | 0x7C00u);
    return (uint16)(sign | ((uint32_t)exp << 10) | (mant >> 13));
}

/**
 * [-1,1] → [0,255] 量化（RG8 法线压缩用——octahedral 编码后写入 uint8）
 */
inline uint8 QuantizeU8(float v)
{
    const int32_t q = (int32_t)roundf(v * 127.5f + 127.5f);
    return (uint8_t)(q < 0 ? 0 : (q > 255 ? 255 : q));
}

/**
 * 法线 octahedral 编码（2 分量保存完整方向——z 符号保留，RG16F/RG8 压缩用）
 * 输出 p,q ∈ [-1,1]；GPU 解码：n = vec3(p, 1-|p|-|q|)；if(n.z<0) n.xy=(1-|n.yx|)*sign(n.xy)；normalize
 */
inline void EncodeOctahedralNormal(float nx, float ny, float nz, float &out_p, float &out_q)
{
    const float len = sqrt(nx * nx + ny * ny + nz * nz);

    if(len < 0.0001f)
    {
        out_p = 0.0f;
        out_q = 0.0f;
        return;
    }

    nx /= len;
    ny /= len;
    nz /= len;

    const float ax = fabsf(nx);
    const float ay = fabsf(ny);
    const float az = fabsf(nz);
    const float l1 = ax + ay + az;

    out_p = nx / l1;
    out_q = ny / l1;

    if(nz < 0.0f)
    {
        // 近纯 -Z 退化：|p'|+|q'|≈0 时折叠公式归零（与 +Z 同编码——解码成 +Z 方向反）
        // 特判编码到折叠角 (1,1)——解码展开后恰好得 (0,0,-1)
        if(out_p * out_p + out_q * out_q < 1e-6f)
        {
            out_p = 1.0f;
            out_q = 1.0f;
        }
        else
        {
            out_p = (1.0f - ay / l1) * (nx >= 0.0f ? 1.0f : -1.0f);
            out_q = (1.0f - ax / l1) * (ny >= 0.0f ? 1.0f : -1.0f);
        }
    }
}

/**
 * 可绘制原始图形创建器
 */
class GeometryCreater
{
protected:

    VulkanDevice *      device;
    BufferManager *     buffer_manager;
    VertexDataManager * vdm;

    GeometryVertexFormat geometry_vertex_format;

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

    const int InitVAB(const VertexSemantic semantic,const VkFormat format,const void *data);                                  ///<取得顶点属性索引

    IndexBuffer * GetIBO();                                                                                                 ///<取得索引缓冲区
    int32_t GetFirstIndex()const;                                                                                           ///<取得第一个索引

public:

    GeometryCreater(VulkanDevice *,const GeometryVertexFormat &,BufferManager *bm=nullptr);
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

    const   uint32_t        GetVertexCount()const{ return vertices_number; }                                                ///<取得顶点数量
        int32_t         GetVertexOffset()const;                                                                         ///<取得顶点偏移(单位:元素)

        VertexAttribBuffer * GetVAB (const VertexSemantic semantic,const VkFormat format=VK_FORMAT_UNDEFINED);       ///<获取VAB用于BufferAccessor

        bool            WriteVAB    (const VertexSemantic semantic,const VkFormat format,const void *data);           ///<直接写入顶点属性数据

        /**
         * 创建带偏移的 BufferAccessor（自动使用VDM子分配的正确范围）
             * @tparam BufferAccessorType BufferAccessor类型（如 BufferAccessor3f）
             * @param name 顶点属性名称
             * @return 已绑定到正确偏移/数量的 BufferAccessor
             */
            template<typename BufferAccessorType>
            BufferAccessorType GetBufferAccessor(const VertexSemantic semantic)
            {
                VAB *vab = GetVAB(semantic);
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
                            const   GeometryVertexFormat &geometry_vertex_format,
                            const   AnsiString &    name,
                            const   uint32_t        vertex_count,
                            const   uint32_t        index_count = 0,
                                    IndexType       it          = IndexType::AUTO,
                                    BufferManager * bm          = nullptr,
                                    BufferAllocPolicy policy    = BufferAllocPolicy::GPUOnly);
}//namespace hgl::graph
