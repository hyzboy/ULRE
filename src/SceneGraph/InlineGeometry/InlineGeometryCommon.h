#pragma once

#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/geo/IndexGenerator.h>
#include "GeometryValidator.h"
#include <algorithm>
#include <cmath>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    // Note: GLUS_* constants are now defined in GeometryValidator.h
    // They are available through the include above

    // ========================================================================
    // Quaternion helper functions for geometry generation
    // ========================================================================
    
    /**
     * Create a quaternion for rotation around Y axis
     * @param quaternion Output quaternion [x,y,z,w]
     * @param angle Rotation angle in degrees
     */
    inline void QuaternionRotateY(float quaternion[4], const float angle)
    {
        float halfAngleRadian = deg2rad(angle) * 0.5f;

        quaternion[0] = 0.0f;
        quaternion[1] = sin(halfAngleRadian);
        quaternion[2] = 0.0f;
        quaternion[3] = cos(halfAngleRadian);
    }

    /**
     * Create a quaternion for rotation around Z axis
     * @param quaternion Output quaternion [x,y,z,w]
     * @param angle Rotation angle in degrees
     */
    inline void QuaternionRotateZ(float quaternion[4], const float angle)
    {
        float halfAngleRadian = deg2rad(angle) * 0.5f;

        quaternion[0] = 0.0f;
        quaternion[1] = 0.0f;
        quaternion[2] = sin(halfAngleRadian);
        quaternion[3] = cos(halfAngleRadian);
    }

    /**
     * Convert quaternion to 4x4 matrix
     * @param matrix Output 16-element matrix (column-major)
     * @param quaternion Input quaternion [x,y,z,w]
     */
    inline void QuaternionToMatrix(float matrix[16], const float quaternion[4])
    {
        float x = quaternion[0];
        float y = quaternion[1];
        float z = quaternion[2];
        float w = quaternion[3];

        matrix[0] = 1.0f - 2.0f * y * y - 2.0f * z * z;
        matrix[1] = 2.0f * x * y + 2.0f * w * z;
        matrix[2] = 2.0f * x * z - 2.0f * w * y;
        matrix[3] = 0.0f;

        matrix[4] = 2.0f * x * y - 2.0f * w * z;
        matrix[5] = 1.0f - 2.0f * x * x - 2.0f * z * z;
        matrix[6] = 2.0f * y * z + 2.0f * w * x;
        matrix[7] = 0.0f;

        matrix[8] = 2.0f * x * z + 2.0f * w * y;
        matrix[9] = 2.0f * y * z - 2.0f * w * x;
        matrix[10] = 1.0f - 2.0f * x * x - 2.0f * y * y;
        matrix[11] = 0.0f;

        matrix[12] = 0.0f;
        matrix[13] = 0.0f;
        matrix[14] = 0.0f;
        matrix[15] = 1.0f;
    }

    /**
     * Multiply 4x4 matrix with 3D vector
     * @param result Output vector
     * @param matrix 16-element matrix (column-major)
     * @param vector Input 3D vector
     */
    inline void MatrixMultiplyVector3(float result[3], const float matrix[16], const float vector[3])
    {
        float temp[3];

        for (int i = 0; i < 3; i++)
        {
            temp[i] = matrix[i] * vector[0] + matrix[4 + i] * vector[1] + matrix[8 + i] * vector[2];
        }

        for (int i = 0; i < 3; i++)
        {
            result[i] = temp[i];
        }
    }

} // namespace hgl::graph::inline_geometry
