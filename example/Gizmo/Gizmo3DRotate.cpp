#include"GizmoResource.h"
#include<hgl/graph/VKRenderResource.h>
#include<hgl/graph/InlineGeometry.h>
#include<hgl/graph/TransformFaceToCamera.h>

VK_NAMESPACE_BEGIN

namespace
{
    static StaticMesh *sm_gizmo_rotate=nullptr;
}//namespace

StaticMesh *GetGizmoRotateStaticMesh()
{
    return sm_gizmo_rotate;
}

void ClearGizmoRotateStaticMesh()
{
    SAFE_CLEAR(sm_gizmo_rotate);
}

bool InitGizmoRotateStaticMesh()
{
    Renderable *torus[4]
    {
        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::Red),
        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::Green),
        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::Blue),

        GetGizmoRenderable(GizmoShape::Torus,GizmoColor::White),
    };

    for(int i=0;i<4;i++)
    {
        if(!torus[i])
            return(false);
    }

    {
        SceneNode *root_node=new SceneNode();

        {
            Transform tm;

            tm.SetScale(GIZMO_ARROW_LENGTH);

            root_node->Add(new SceneNode(tm,torus[0]));

            tm.SetRotation(AXIS::Z,90);
            root_node->Add(new SceneNode(tm,torus[1]));

            tm.SetRotation(AXIS::Y,90);
            root_node->Add(new SceneNode(tm,torus[2]));
        }

        {
            SceneNode *white_torus=new SceneNode(scale(13),torus[3]);

            white_torus->SetLocalNormal(AxisVector::X);

            TransformFaceToCamera *rotate_white_torus_tfc=new TransformFaceToCamera();
            
            //暂时因为无法传入Camera所以无法正确计算朝向，正在设计Actor/World结构

            white_torus->GetTransform().AddTransform(rotate_white_torus_tfc);

            root_node->Add(white_torus);
        }

        sm_gizmo_rotate=CreateGizmoStaticMesh(root_node);
    }

    if(!sm_gizmo_rotate)
        return(false);

    return(true);
}

VK_NAMESPACE_END
