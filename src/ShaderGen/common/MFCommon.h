#pragma once

#include<hgl/graph/mtl/StdMaterial.h>

STD_MTL_FUNC_NAMESPACE_BEGIN
//C++端使用一个RG8UI或RGB16UI格式的顶点输入流来传递Assign数据，其中x为LocalToWorld ID，y为MaterialInstance ID
    
    constexpr const char MF_GetLocalToWorld_ByAssign[]=     "\nmat4 GetLocalToWorld(){return l2w.mats[Assign.x];}\n";

    constexpr const char MF_GetMaterialInstance_ByAssign[]= "\nMaterialInstance GetMaterialInstance(){return mi_set[Assign.y];}\n";

STD_MTL_FUNC_NAMESPACE_END
