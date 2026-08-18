#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/type/BlockAllocator.h>
#include<hgl/log/Logger.h>

namespace hgl::graph{

class BufferManager;

/**
 * 顶点数据管理器——所有模型共享的大 VBO/SSBO（超级大缓冲）
 *
 * 设计：每个顶点语义（Position/UV/NTB/...）一个 VAB（大 buffer），所有模型的数据
 * 通过 BlockAllocator 分段写入（AcquireVAB 分配段，GetStart() 即段起始顶点号）。
 * 模型的顶点偏移 = vab_node->GetStart()（顶点号，非字节）——渲染时作为
 * vkCmdDrawIndexed 的 firstVertex（vertexOffset）传给绘制命令。
 *
 * 顶点偏移计算（VBO 与 SSBO 顶点输入统一语义）：
 * - VBO 模式（传统 attribute fetch）：硬件自动用 firstVertex + 索引取顶点——
 *   shader 无需任何处理（attribute 已按绝对顶点寻址）。
 * - SSBO 顶点输入模式（MeshShader 方向，s1_* 模块）：
 *   Vulkan 的 gl_VertexIndex（SPIR-V VertexIndex）在 indexed draw 中 =
 *   BaseVertex + index buffer 值（firstVertex 自动含入！）——
 *   shader 直接 data[gl_VertexIndex] 即 VDM 大 buffer 的绝对顶点号，
 *   不要再加 gl_BaseVertex/gl_BaseVertexARB（会双加错位——多对象场景
 *   段偏移≠0 时暴露，RenderDoc 数据铁证：floor 段 0 → IDX 0..3、
 *   cube 段 4 → IDX 4..27、cone 段 28 → IDX 28..40）。
 * - 布局注意：VAB 数据是紧凑格式（如 vec3 = 12B/顶点）——SSBO 声明必须用
 *   layout(std430, scalar)（GL_EXT_scalar_block_layout）——std430 默认的
 *   vec3 数组 stride 16B 会错位（SimpleCube 顶点混乱的根因）。
 * - 验证教训：单对象（BaseVertex=0）验证通过≠多对象正确——VDM 多对象
 *   场景是顶点偏移类 bug 的必测项。
 */
class VertexDataManager
{
    OBJECT_LOGGER

    VulkanDevice *device;
    BufferManager *buffer_manager;

    // TODO: Migrate VAB/IBO allocations to BufferManager while keeping pooled behavior.

protected:

    GeometryVertexFormat geometry_vertex_format;
          uint      vi_count;       ///<顶点输入流数量

    VkDeviceSize    vab_max_size;   ///<顶点缓冲区分配空间大小(顶点数)
    VkDeviceSize    vab_cur_size;   ///<顶点缓冲区当前使用大小
    VAB **          vab;            ///<顶点缓冲区列表

    VkDeviceSize    ibo_cur_size;   ///<索引缓冲区当前使用大小
    IndexBuffer *   ibo;            ///<索引缓冲区

protected:

    BlockAllocator       vbo_data_chain; ///<数据链
    BlockAllocator       ibo_data_chain; ///<数据链

public:

    VertexDataManager(VulkanDevice *dev,const GeometryVertexFormat &gvf);
    VertexDataManager(BufferManager *bm,const GeometryVertexFormat &gvf);
    ~VertexDataManager();

          VulkanDevice *GetDevice       ()const{return device;}                                     ///<取得GPU设备
          BufferManager *GetBufferManager()const{return buffer_manager;}                             ///<取得BufferManager

    const GeometryVertexFormat &GetGeometryVertexFormat()const{return geometry_vertex_format;}

    const VkDeviceSize  GetVABMaxCount  ()const{return vab_max_size;}                                ///<取得顶点属性缓冲区分配的空间最大数量
    const VkDeviceSize  GetVABCurCount  ()const{return vab_cur_size;}                                ///<取得顶点属性缓冲区当前数量

    const IndexType     GetIndexType      ()const{return ibo?ibo->GetType():IndexType::ERR;}         ///<取得索引缓冲区类型
    const VkDeviceSize  GetIndexMaxCount  ()const{return ibo?ibo->GetCount():-1;}                    ///<取得索引缓冲区分配的空间最大数量
    const VkDeviceSize  GetIndexCurCount  ()const{return ibo?ibo_cur_size:-1;}                       ///<取得索引缓冲区当前数量

public:

    bool Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type);

    BlockAllocator::UserNode *AcquireIB(const VkDeviceSize count);
    BlockAllocator::UserNode *AcquireVAB(const VkDeviceSize count);

    bool ReleaseIB(BlockAllocator::UserNode *);
    bool ReleaseVAB(BlockAllocator::UserNode *);

    IndexBuffer *GetIBO(){return ibo;}
    VAB *GetVAB(const uint index){return vab[index];}
};//class VertexDataManager

using VDM=VertexDataManager;
}//namespace hgl::graph
