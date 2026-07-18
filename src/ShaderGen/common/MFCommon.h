#pragma once

#include<hgl/common/RenderAssignDef.h>
#include<hgl/mtl/StdMaterial.h>

namespace hgl::graph::mtl::func
{
// ═════════════════════════════════════════════════════════════════════════════
// MaterialInstance helper 统一入口（符合 SHADER_HELPER_FUNCTION_SPEC.md）
// ═════════════════════════════════════════════════════════════════════════════
// 规范约束：
//   1) 新增材质逻辑统一使用 GetMI()
//   2) Vertex/Fragment/Geometry 各阶段名称保持一致，只允许索引来源不同
//   3) GetMaterialInstance() 仅作为旧 helper 别名保留，不再扩展

/// 统一的 MaterialInstance 获取函数（唯一标准名称）
/// Vertex Shader: 通过 rows SSBO + gl_InstanceIndex 索引
constexpr const char MF_GetMI_VS[] = 
    "\nMaterialInstance GetMI(){return mtl.mi[ResolveDataIndexID(gl_InstanceIndex)];}\n";

/// Fragment/Geometry Shader: 从插值输入 Input.DataIndexID 索引
constexpr const char MF_GetMI_Other[] = 
    "\nMaterialInstance GetMI(){return mtl.mi[Input.DataIndexID];}\n";

/// 规范映射（文档/代码统一口径）
///   Canonical: GetMI()
///   Alias    : GetMaterialInstance()（仅旧 helper 别名）

// ═════════════════════════════════════════════════════════════════════════════
// 旧 helper 别名区
// ═════════════════════════════════════════════════════════════════════════════

// C++端使用一个RG8UI或RGB16UI格式的顶点输入流来传递Assign数据，其中x为LocalToWorld ID，y为MaterialInstance ID

constexpr const char MF_GetLocalToWorld_ByAssign[] =
    "\nmat4 GetLocalToWorld(){return l2w.mats[ResolveTransformID(gl_InstanceIndex)];}\n";

constexpr const char MI_ID_OUTPUT[] = "DataIndexID";

constexpr const char MF_HandoverMI_VS[] = 
    "\nvoid HandoverMI(){Output.DataIndexID=ResolveDataIndexID(gl_InstanceIndex);}\n";
constexpr const char MF_HandoverMI_GS[] = 
    "\nvoid HandoverMI(){Output.DataIndexID=Input[0].DataIndexID;}\n";
constexpr const char MF_HandoverMI_OTHER[] = 
    "\nvoid HandoverMI(){Output.DataIndexID=Input.DataIndexID;}\n";

}//namespace hgl::graph::mtl::func
