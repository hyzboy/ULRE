#include<hgl/graph/SceneMatrix.h>

namespace hgl
{
    namespace graph
    {
        SceneMatrix::SceneMatrix(SceneMatrix &so):VersionData(so.GetLocalToWorldMatrix())
        {
            parent_matrix=so.parent_matrix;
            local_matrix=so.local_matrix;
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
            transform_matrix=Identity4f;
            UpdateVersion();
        }
        
        void SceneMatrix::MakeNewestData(Matrix4f &local_to_world_matrix)
        {
            local_to_world_matrix=parent_matrix*local_matrix;

            OriginWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);

            if(transform_manager.IsEmpty())
            {
                FinalWorldPosition=OriginWorldPosition;
            }
            else
            {
                transform_manager.GetMatrix(transform_matrix,OriginWorldPosition);

                local_to_world_matrix*=transform_matrix;

                FinalWorldPosition=TransformPosition(local_to_world_matrix,ZeroVector3f);
            }
            
            inverse_local_to_world_matrix          =inverse(local_to_world_matrix);
            inverse_transpose_local_to_world_matrix=transpose(inverse_local_to_world_matrix);
        }
    }//namespace graph
}//namespace hgl
