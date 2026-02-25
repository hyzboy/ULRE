#pragma once

#include<hgl/vk/VKRenderAssign.h>
#include<hgl/graph/mtl/StdMaterial.h>

namespace hgl::graph::mtl::func
{
// ═════════════════════════════════════════════════════════════════════════════
// Phase B 规范化 MaterialInstance Helper（强制使用，符合 SHADER_HELPER_FUNCTION_SPEC.md）
// ═════════════════════════════════════════════════════════════════════════════

/// 统一的 MaterialInstance 获取函数（唯一标准名称）
/// Vertex Shader: 直接从顶点属性 MaterialInstanceID 索引
constexpr const char MF_GetMI_VS[] = 
    "\nMaterialInstance GetMI(){return mtl.mi[MaterialInstanceID];}\n";

/// Fragment/Geometry Shader: 从插值输入 Input.MaterialInstanceID 索引
constexpr const char MF_GetMI_Other[] = 
    "\nMaterialInstance GetMI(){return mtl.mi[Input.MaterialInstanceID];}\n";

// ═════════════════════════════════════════════════════════════════════════════
// Legacy Helper（Phase C 前保留兼容性）
// ═════════════════════════════════════════════════════════════════════════════

// C++端使用一个RG8UI或RGB16UI格式的顶点输入流来传递Assign数据，其中x为LocalToWorld ID，y为MaterialInstance ID

constexpr const char MF_GetLocalToWorld_ByAssign[] = 
    "\nmat4 GetLocalToWorld(){return l2w.mats[TransformID];}\n";

// DEPRECATED: 使用 MF_GetMI_VS / MF_GetMI_Other 替代（名称过长）
constexpr const char MF_GetMaterialInstance_ByAssign[] = 
    "\nMaterialInstance GetMaterialInstance(){return mi_set[MaterialInstanceID];}\n";

constexpr const char MI_ID_OUTPUT[] = "MaterialInstanceID";

constexpr const char MF_HandoverMI_VS[] = 
    "\nvoid HandoverMI(){Output.MaterialInstanceID=MaterialInstanceID;}\n";
constexpr const char MF_HandoverMI_GS[] = 
    "\nvoid HandoverMI(){Output.MaterialInstanceID=Input[0].MaterialInstanceID;}\n";
constexpr const char MF_HandoverMI_OTHER[] = 
    "\nvoid HandoverMI(){Output.MaterialInstanceID=Input.MaterialInstanceID;}\n";

}//namespace hgl::graph::mtl::func
