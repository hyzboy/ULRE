#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/graph/mesh/StaticMeshLODPolicy.h>
#include<hgl/graph/ShadowPolicy.h>

VK_NAMESPACE_BEGIN

class SceneNode;

class StaticMesh
{
protected:

    StaticMeshLODPolicy lod_policy;                                             ///<LOD策略

    SceneNode *root_node;

    StaticMesh *shadow_proxy_static_mesh;                                       ///<阴影代理静态网格
    StaticMesh *physic_proxy_static_mesh;                                       ///<物理代理静态网格

protected:

    bool two_side;                                                              ///<双面渲染

    ObjectDynamicShadowPolicy recommend_dynamic_shadow_policy;                  ///<动态阴影策略(推荐项,最终可被取代)

public:

    const StaticMeshLODPolicy       GetLODPolicy()const { return lod_policy; }                                          ///<取得LOD策略
    const ObjectDynamicShadowPolicy GetRecommendDynamicShadowPolicy()const { return recommend_dynamic_shadow_policy; }  ///<取得推荐的动态阴影策略

public:

    StaticMesh(SceneNode *);
    virtual ~StaticMesh();

public:

    SceneNode *GetScene(){return root_node;}

    SceneNode *GetShadowNode() { return shadow_proxy_static_mesh?shadow_proxy_static_mesh->GetScene():root_node; }      ///<取得阴影渲染节点
    SceneNode *GetPhysicNode() { return physic_proxy_static_mesh?physic_proxy_static_mesh->GetScene():root_node; }      ///<取得物理渲染节点

};//class StaticMesh
VK_NAMESPACE_END
