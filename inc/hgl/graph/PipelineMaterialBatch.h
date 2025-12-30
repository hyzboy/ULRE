#pragma once
#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VKIndirectCommandBuffer.h>
#include<hgl/type/ArrayList.h>
#include<hgl/component/Component.h>

COMPONENT_NAMESPACE_BEGIN
class RenderComponent;
class PrimitiveComponent;
COMPONENT_NAMESPACE_END

VK_NAMESPACE_BEGIN
class InstanceAssignmentBuffer;
class SceneNode;
struct CameraInfo;

struct PipelineMaterialIndex:public Comparator<PipelineMaterialIndex>
{
    Material *material;
    Pipeline *pipeline;

public:

    const int compare(const PipelineMaterialIndex &rli)const override
    {
        if(material<rli.material)return(-1);
        if(material>rli.material)return(1);

        if(pipeline<rli.pipeline)return(-1);
        if(pipeline>rli.pipeline)return(1);

        return(0);
    }

public:

    PipelineMaterialIndex()
    {
        material=nullptr;
        pipeline=nullptr;
    }

    PipelineMaterialIndex(Material *m,Pipeline *p)
    {
        material=m;
        pipeline=p;
    }
};//struct PipelineMaterialIndex

/**
* 同一材质与管线的渲染列表
* 
* 职责：
* - 批量管理使用相同 Material 和 Pipeline 的渲染节点
* - 组织和优化绘制调用（Draw Call Batching）
* - 管理实例数据（LocalToWorld 矩阵、材质实例数据）
* - 支持直接绘制和间接绘制（Indirect Drawing）
*/
class PipelineMaterialBatch
{
public:
    /**
     * 绘制批次：将使用相同几何数据的节点合并为一个批次
     */
    struct DrawBatch
    {
                uint32_t                first_instance;     ///<第一个绘制实例(和instance渲染无关,对应InstanceRate的VAB)
                uint32_t                instance_count;     ///<此批次包含的实例数量

        const   GeometryDataBuffer *    geom_data_buffer;   ///<几何数据缓冲
        const   GeometryDrawRange *     geom_draw_range;    ///<绘制范围（顶点/索引偏移和数量）

        void Set(Primitive *);
    };

private:
    // === 核心标识 ===
    VulkanDevice *device;                       ///<Vulkan设备
    PipelineMaterialIndex pm_index;             ///<Pipeline和Material索引
    const CameraInfo *camera_info;              ///<相机信息（用于距离排序）

    // === 渲染节点管理 ===
    DrawNodeList draw_nodes;                    ///<所有渲染节点
    DrawNodePointerList transform_dirty_nodes;  ///<变换已修改的节点列表

    // === 实例数据管理 ===
    InstanceAssignmentBuffer *assign_buffer;    ///<实例分配缓冲（LocalToWorld/MI数据）

    // === 批次管理 ===
    DataArray<DrawBatch> draw_batches;          ///<绘制批次数组
    uint draw_batches_count;                    ///<有效批次数量

    // === 间接绘制缓冲 ===
    IndirectDrawBuffer *icb_draw;               ///<间接绘制命令缓冲（无索引）
    IndirectDrawIndexedBuffer *icb_draw_indexed;///<间接绘制命令缓冲（有索引）

    // === 渲染状态缓存 ===
    RenderCmdBuffer *cmd_buf;                   ///<当前渲染命令缓冲
    VABList *vab_list;                          ///<顶点属性缓冲列表

    const GeometryDataBuffer *last_data_buffer; ///<上次绑定的几何数据缓冲
    const VDM *last_vdm;                        ///<上次使用的顶点数据管理器
    const GeometryDrawRange *last_draw_range;   ///<上次使用的绘制范围

    int first_indirect_draw_index;              ///<首个间接绘制索引
    uint indirect_draw_count;                   ///<累积的间接绘制数量

    // === 批次构建辅助方法 ===
    void ReallocICB();                          ///<重新分配间接绘制缓冲
    void WriteICB(VkDrawIndirectCommand *, DrawBatch *);
    void WriteICB(VkDrawIndexedIndirectCommand *, DrawBatch *);
    void BuildBatches();                        ///<构建绘制批次

    // === 渲染辅助方法 ===
    bool BindVAB(const DrawBatch *);            ///<绑定顶点属性缓冲
    void ProcIndirectRender();                  ///<处理间接渲染
    bool Draw(DrawBatch *);                     ///<绘制单个批次

public:
    // === 生命周期管理 ===
    PipelineMaterialBatch(VulkanDevice *d, bool l2w, const PipelineMaterialIndex &rpi);
    ~PipelineMaterialBatch();

    // === 节点管理接口 ===
    void Add(DrawNode *node);                   ///<添加渲染节点
    
    void Clear()                                ///<清空所有节点
    {
        for (auto *n : draw_nodes) delete n;
        draw_nodes.Clear();
    }

    // === 配置接口 ===
    void SetCameraInfo(const CameraInfo *ci) { camera_info = ci; }  ///<设置相机信息

    // === 构建和准备 ===
    void Finalize();                            ///<完成节点添加，准备渲染

    // === 数据更新接口 ===
    void UpdateTransformData();                 ///<刷新所有对象的LocalToWorld矩阵
    void UpdateMaterialInstanceData(PrimitiveComponent *);  ///<更新材质实例数据

    // === 渲染执行 ===
    void Render(RenderCmdBuffer *);             ///<执行渲染
};//class PipelineMaterialBatch
VK_NAMESPACE_END
