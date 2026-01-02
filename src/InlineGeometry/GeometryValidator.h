#pragma once

#include<hgl/graph/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    // Shared constants used by multiple geometry creators
    inline constexpr uint GLUS_VERTICES_FACTOR =4;
    inline constexpr uint GLUS_VERTICES_DIVISOR=4;
    inline constexpr uint GLUS_MAX_VERTICES    =1048576;
    inline constexpr uint GLUS_MAX_INDICES     =GLUS_MAX_VERTICES*GLUS_VERTICES_FACTOR;

    /**
     * 几何体参数验证器
     * 提供统一的参数验证函数，避免重复代码
     */
    namespace GeometryValidator
    {
        /**
         * 验证基本参数
         * @param pc GeometryCreater指针
         * @param numberVertices 顶点数量
         * @param numberIndices 索引数量
         * @return 如果参数有效返回true，否则返回false
         */
        inline bool ValidateBasicParams(GeometryCreater *pc, uint numberVertices, uint numberIndices)
        {
            if(!pc)
                return false;
                
            if(numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
                return false;
                
            return true;
        }

        /**
         * 验证切片数量（用于Sphere等只需要切片参数的几何体）
         * @param slices 切片数量
         * @param minSlices 最小切片数量（默认3）
         * @return 如果参数有效返回true，否则返回false
         */
        inline bool ValidateSlices(uint slices, uint minSlices = 3)
        {
            return slices >= minSlices;
        }

        /**
         * 验证切片和堆栈参数（用于Cylinder, Cone, Torus等需要切片和堆栈的几何体）
         * @param slices 切片数量
         * @param stacks 堆栈数量
         * @param minSlices 最小切片数量（默认3）
         * @param minStacks 最小堆栈数量（默认1）
         * @return 如果参数有效返回true，否则返回false
         */
        inline bool ValidateSlicesAndStacks(uint slices, uint stacks, uint minSlices = 3, uint minStacks = 1)
        {
            return slices >= minSlices && stacks >= minStacks;
        }
    }
}
