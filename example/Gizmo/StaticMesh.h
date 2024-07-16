#pragma once

#include<hgl/graph/VKRenderable.h>
#include<hgl/type/SortedSets.h>
#include<hgl/graph/SceneNode.h>

VK_NAMESPACE_BEGIN

class StaticMesh
{
    RenderResource *rr;

    SortedSets<Primitive *> prim_set;
    SortedSets<MaterialInstance *> mi_set;
    SortedSets<Pipeline *> pipeline_set;

protected:

    SceneNode *root_node

public:


};

VK_NAMESPACE_END
