#pragma once

#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/geo/GeometryMath.h>
#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/geo/IndexGenerator.h>
#include "GeometryValidator.h"
#include <algorithm>
#include <cmath>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    // Shared constants (moved to GeometryValidator.h, kept here for backward compatibility)
    constexpr uint GLUS_VERTICES_FACTOR =4;
    constexpr uint GLUS_VERTICES_DIVISOR=4;
    constexpr uint GLUS_MAX_VERTICES    =1048576;
    constexpr uint GLUS_MAX_INDICES     =GLUS_MAX_VERTICES*GLUS_VERTICES_FACTOR;

    // ========================================================================
    // DEPRECATED: Legacy functions - Use GeometryMath namespace instead
    // ========================================================================
    
    // Deprecated: Use GeometryMath::QuaternionRotateY instead
    inline void glusQuaternionRotateRyf(float quaternion[4], const float angle)
    {
        GeometryMath::QuaternionRotateY(quaternion, angle);
    }

    // Deprecated: Use GeometryMath::QuaternionRotateZ instead
    inline void glusQuaternionRotateRzf(float quaternion[4], const float angle)
    {
        GeometryMath::QuaternionRotateZ(quaternion, angle);
    }

    // Deprecated: Use GeometryMath::QuaternionToMatrix instead
    inline void glusQuaternionGetMatrix4x4f(float matrix[16], const float quaternion[4])
    {
        GeometryMath::QuaternionToMatrix(matrix, quaternion);
    }

    // Deprecated: Use GeometryMath::MatrixMultiplyVector3 instead
    inline void glusMatrix4x4MultiplyVector3f(float result[3], const float matrix[16], const float vector[3])
    {
        GeometryMath::MatrixMultiplyVector3(result, matrix, vector);
    }

    // Deprecated: Use IndexGenerator::WriteSequentialIndices instead
    template<typename T>
    void WriteIBO(T *p,const T start,const T count)
    {
        IndexGenerator::WriteSequentialIndices(p, start, count);
    }

    // Deprecated: Use IndexGenerator::WriteCircleIndices instead
    template<typename T>
    void WriteCircleIBO(T *ibo,const uint edge_count)
    {
        IndexGenerator::WriteCircleIndices(ibo, edge_count);
    }

    // Deprecated: Use IndexGenerator::CreateSphereIndices instead
    template<typename T>
    void CreateSphereIndices(GeometryCreater *pc,uint numberParallels,const uint numberSlices)
    {
        IndexGenerator::CreateSphereIndices<T>(pc, numberParallels, numberSlices);
    }

    // Deprecated: Use IndexGenerator::CreateTorusIndices instead
    template<typename T>
    void CreateTorusIndices(GeometryCreater *pc,uint numberSlices,uint numberStacks)
    {
        IndexGenerator::CreateTorusIndices<T>(pc, numberSlices, numberStacks);
    }

    // Deprecated: Use IndexGenerator::CreateCylinderIndices instead
    template<typename T>
    void CreateCylinderIndices(GeometryCreater *pc,const uint numberSlices)
    {
        IndexGenerator::CreateCylinderIndices<T>(pc, numberSlices);
    }

    // Deprecated: Use IndexGenerator::CreateConeIndices instead
    template<typename T>
    void CreateConeIndices(GeometryCreater *pc,const uint numberSlices,const uint numberStacks)
    {
        IndexGenerator::CreateConeIndices<T>(pc, numberSlices, numberStacks);
    }

} // namespace hgl::graph::inline_geometry
