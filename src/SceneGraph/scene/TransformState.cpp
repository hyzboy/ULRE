#include<hgl/graph/TransformState.h>

namespace hgl::math
{
    TransformState::TransformState(TransformState &so):VersionData(so.GetLocalToWorldMatrix())
    {
        parent_matrix=so.parent_matrix;
        local_matrix=so.local_matrix;
        local_is_identity=IsIdentityMatrix(local_matrix);
        local_normal=AxisVector::Z;
        transform_manager=so.transform_manager;
        transform_matrix=so.transform_matrix;

        inverse_local_to_world_matrix=so.inverse_local_to_world_matrix;
        inverse_transpose_local_to_world_matrix=so.inverse_transpose_local_to_world_matrix;
        UpdateVersion();
    }

    void TransformState::Reset()
    {
        parent_matrix=Identity4f;
        local_matrix=Identity4f;
        local_is_identity=true;
        local_normal=AxisVector::Z;
        transform_matrix=Identity4f;
        UpdateVersion();
    }
        
    void TransformState::MakeNewestData(Matrix4f &local_to_world_matrix)
    {
        if(local_is_identity)
            local_to_world_matrix=parent_matrix;
        else
            local_to_world_matrix=parent_matrix*local_matrix;

        PrevWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);
        PrevWorldNormal=TransformNormal(local_to_world_matrix,local_normal);

        if(transform_manager.IsEmpty())
        {
            WorldPosition=PrevWorldPosition;
            WorldNormal=PrevWorldNormal;
        }
        else
        {
            transform_manager.GetMatrix(transform_matrix,PrevWorldPosition,PrevWorldNormal);

            local_to_world_matrix*=transform_matrix;

            WorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);
            WorldNormal=TransformNormal(local_to_world_matrix,local_normal);
        }
            
        inverse_local_to_world_matrix          =Inverse(local_to_world_matrix);
        inverse_transpose_local_to_world_matrix=Transpose(inverse_local_to_world_matrix);
    }

    void TransformState::Update()
    {
        if(transform_manager.IsEmpty())
        {
            if(IsNewestVersion())
                return;
        }
        else
        {
            if(!transform_manager.Update())
                return;

            UpdateVersion();
        }

        UpdateNewestData();
    }
}//namespace hgl::math
