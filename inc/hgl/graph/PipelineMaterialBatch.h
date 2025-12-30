#pragma once
#include<hgl/graph/DrawNode.h>
#include<hgl/graph/PipelineMaterialRenderer.h>
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
* 同一材质与管线的渲染批次管理器
* 
* 职责：
* - 收集和管理使用相同 Material 和 Pipeline 的渲染节点
* - 排序和组织渲染节点
* - 构建绘制批次（Draw Call Batching）
* - 管理实例数据（LocalToWorld 矩阵、材质实例数据）
* - 分配和管理间接绘制缓冲
*/
class PipelineMaterialBatch
{
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
    DrawBatchArray draw_batches;                ///<绘制批次数组
    uint draw_batches_count;                    ///<有效批次数量

    // === 间接绘制缓冲 ===
    IndirectDrawBuffer *icb_draw;               ///<间接绘制命令缓冲（无索引）
    IndirectDrawIndexedBuffer *icb_draw_indexed;///<间接绘制命令缓冲（有索引）

    // === 渲染器 ===
    PipelineMaterialRenderer *renderer;         ///<渲染器实例

    // === 批次构建辅助方法 ===
    void ReallocICB();                          ///<重新分配间接绘制缓冲
    void WriteICB(VkDrawIndirectCommand *, DrawBatch *);
    void WriteICB(VkDrawIndexedIndirectCommand *, DrawBatch *);
    void BuildBatches();                        ///<构建绘制批次

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
