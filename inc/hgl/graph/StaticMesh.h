#pragma once

#include<hgl/graph/VKRenderable.h>
#include<hgl/type/SortedSets.h>
#include<hgl/graph/SceneNode.h>

VK_NAMESPACE_BEGIN
class StaticMesh
{
protected:
    
    SceneNode *root_node;

public:

    StaticMesh(SceneNode *);
    virtual ~StaticMesh();

//    static StaticMesh *CreateNewObject(RenderResource *,SceneNode *);

public:

    SceneNode *GetScene(){return root_node;}
};//class StaticMesh
VK_NAMESPACE_END
