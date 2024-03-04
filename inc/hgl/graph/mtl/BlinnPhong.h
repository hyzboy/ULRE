#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/math/Vector.h>

namespace hgl
{
    namespace graph
    {
        namespace mtl
        {
            namespace blinnphong
            {
                struct SunLight
                {
                    Vector3f direction;
                    Vector3f color;
                };//struct SunLight
            }//namespace blinnphong
        }//namespace mtl
    }//namespace graph
}//namespace hgl