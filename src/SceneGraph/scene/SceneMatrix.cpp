#include<hgl/graph/SceneMatrix.h>

namespace hgl
{
    namespace graph
    {
        SceneMatrix::SceneMatrix(SceneMatrix &so):VersionData(so.GetLocalToWorldMatrix())
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

        void SceneMatrix::Clear()
        {
            parent_matrix=Identity4f;
            local_matrix=Identity4f;
            local_is_identity=true;
            local_normal=AxisVector::Z;
            transform_matrix=Identity4f;
            UpdateVersion();
        }
        
        void SceneMatrix::MakeNewestData(Matrix4f &local_to_world_matrix)
        {
            if(local_is_identity)
                local_to_world_matrix=parent_matrix;
            else
                local_to_world_matrix=parent_matrix*local_matrix;

            OriginWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);
            OriginWorldNormal=TransformNormal(local_to_world_matrix,local_normal);

            if(transform_manager.IsEmpty())
            {
                FinalWorldPosition=OriginWorldPosition;
                FinalWorldNormal=OriginWorldNormal;
            }
            else
            {
                transform_manager.GetMatrix(transform_matrix,OriginWorldPosition,OriginWorldNormal);

                local_to_world_matrix*=transform_matrix;

                FinalWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);
                FinalWorldNormal=TransformNormal(local_to_world_matrix,local_normal);
            }
            
            inverse_local_to_world_matrix          =inverse(local_to_world_matrix);
            inverse_transpose_local_to_world_matrix=transpose(inverse_local_to_world_matrix);
        }

        void SceneMatrix::Update()
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
    }//namespace graph
}//namespace hgl
