#pragma once

#include<hgl/graph/Component.h>
#include<hgl/graph/mesh/StaticMesh.h>

VK_NAMESPACE_BEGIN

class StaticMeshComponentData:public ComponentData
{
    StaticMesh *static_mesh;
};//class StaticMeshComponentData:public ComponentData

class StaticMeshComponent:public Component
{


};//class StaticMeshComponent:public Component

VK_NAMESPACE_END
