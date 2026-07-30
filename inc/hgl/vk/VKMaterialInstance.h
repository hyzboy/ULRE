#pragma once

#include<hgl/vk/VKShaderProgram.h>

namespace hgl::graph{

/**
* MaterialInstance — 材质 VIL 持有者（Phase 3 精简后）
*
* 历史上 MaterialInstance 同时持有：
*   1. ShaderProgram 合约签发的 VIL（顶点输入布局）
*   2. ShaderProgram 内部分配的 per-instance 数据区（mi_id → mi_data_manager）
*
* Phase 3 后，per-instance 数据区已完全迁移到外部 SSBO + DescriptorBindingSet。
* MaterialInstance 现在只是 (ShaderProgram*, VIL*) 的轻量包装，mi_id 恒为 -1。
*
* 新代码请使用 DescriptorBindingSet 路径。MaterialInstance 保留是为了：
*   - 旧 API 签名兼容（LoadStaticMeshScene 等）
*   - 非 SSBO 材质（VertexColor2D/3D 等）的 VIL 快速创建
*/
class MaterialInstance
{
protected:

    ShaderProgram *material;

    const VIL *vil;

    int mi_id;  ///< 恒为 -1（Phase 3 后 ShaderProgram 不再分配数据槽）

public:

            ShaderProgram *  GetMaterialProgram ()      {return material;}

    const   VIL *       GetVIL      ()const {return vil;}

private:

    friend class ShaderProgram;

    MaterialInstance(ShaderProgram *,const VIL *,const int);

public:

    virtual ~MaterialInstance()
    {
        // mi_id is always -1 in the new path (no ShaderProgram-owned data store)
    }

    const int GetMIID() const { return mi_id; }
};//class MaterialInstance
}//namespace hgl::graph
