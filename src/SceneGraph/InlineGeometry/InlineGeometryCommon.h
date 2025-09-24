#pragma once

#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include <algorithm>
#include <cmath>

namespace hgl::graph::inline_geometry
{
    // Shared constants used by multiple geometry creators
    constexpr uint GLUS_VERTICES_FACTOR =4;
    constexpr uint GLUS_VERTICES_DIVISOR=4;
    constexpr uint GLUS_MAX_VERTICES    =1048576;
    constexpr uint GLUS_MAX_INDICES     =GLUS_MAX_VERTICES*GLUS_VERTICES_FACTOR;

    inline void glusQuaternionRotateRyf(float quaternion[4], const float angle)
    {
        float halfAngleRadian = deg2rad(angle) * 0.5f;

        quaternion[0] = 0.0f;
        quaternion[1] = sin(halfAngleRadian);
        quaternion[2] = 0.0f;
        quaternion[3] = cos(halfAngleRadian);
    }

    inline void glusQuaternionRotateRzf(float quaternion[4], const float angle)
    {
        float halfAngleRadian = deg2rad(angle) * 0.5f;

        quaternion[0] = 0.0f;
        quaternion[1] = 0.0f;
        quaternion[2] = sin(halfAngleRadian);
        quaternion[3] = cos(halfAngleRadian);
    }

    inline void glusQuaternionGetMatrix4x4f(float matrix[16], const float quaternion[4])
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

    inline void glusMatrix4x4MultiplyVector3f(float result[3], const float matrix[16], const float vector[3])
    {
        int i;

        float temp[3];

        for (i = 0; i < 3; i++)
        {
            temp[i] = matrix[i] * vector[0] + matrix[4 + i] * vector[1] + matrix[8 + i] * vector[2];
        }

        for (i = 0; i < 3; i++)
        {
            result[i] = temp[i];
        }
    }

    template<typename T>
    void WriteIBO(T *p,const T start,const T count)
    {
        for(T i=start;i<start+count;i++)
        {
            *p=i;
            ++p;
        }
    }

    template<typename T>
    void WriteCircleIBO(T *ibo,const uint edge_count)
    {
        T *p=ibo;

        for(T i=1;i<edge_count;i++)
        {
            *p=0;  ++p;
            *p=i+1;++p;
            *p=i;  ++p;
        }

        *p=0;           ++p;
        *p=1;           ++p;
        *p=edge_count;
    }

    template<typename T>
    void CreateSphereIndices(GeometryCreater *pc,uint numberParallels,const uint numberSlices)
    {
        IBTypeMap<T> ib_map(pc->GetIBMap());
        T *tp=ib_map;

        for (uint i = 0; i < numberParallels; i++)
        {
            for (uint j = 0; j < numberSlices; j++)
            {
                *tp= i      * (numberSlices + 1) + j;       ++tp;
                *tp=(i + 1) * (numberSlices + 1) + (j + 1); ++tp;
                *tp=(i + 1) * (numberSlices + 1) + j;       ++tp;

                *tp= i      * (numberSlices + 1) + j;       ++tp;
                *tp= i      * (numberSlices + 1) + (j + 1); ++tp;
                *tp=(i + 1) * (numberSlices + 1) + (j + 1); ++tp;
            }
        }
    }

    template<typename T>
    void CreateTorusIndices(GeometryCreater *pc,uint numberSlices,uint numberStacks)
    {
        IBTypeMap<T> ib_map(pc->GetIBMap());
        T *tp=ib_map;

        // loop counters
        uint sideCount, faceCount;

        // used to generate the indices
        uint v0, v1, v2, v3;

        for (sideCount = 0; sideCount < numberSlices; ++sideCount)
        {
            for (faceCount = 0; faceCount < numberStacks; ++faceCount)
            {
                v0 =  ((sideCount       * (numberStacks + 1)) +  faceCount);
                v1 = (((sideCount + 1)  * (numberStacks + 1)) +  faceCount);
                v2 = (((sideCount + 1)  * (numberStacks + 1)) + (faceCount + 1));
                v3 =  ((sideCount       * (numberStacks + 1)) + (faceCount + 1));

                *tp = v0; ++tp;
                *tp = v2; ++tp;
                *tp = v1; ++tp;

                *tp = v0; ++tp;
                *tp = v3; ++tp;
                *tp = v2; ++tp;
            }
        }
    }

    template<typename T>
    void CreateCylinderIndices(GeometryCreater *pc,const uint numberSlices)
    {
        IBTypeMap<T> ib_map(pc->GetIBMap());
        T *tp=ib_map;
        uint i;

        T centerIndex = 0;
        T indexCounter = 1;

        for (i = 0; i < numberSlices; i++)
        {
            *tp = centerIndex;      ++tp;
            *tp = indexCounter;     ++tp;
            *tp = indexCounter + 1; ++tp;

            indexCounter++;
        }
        indexCounter++;

        // Top
        centerIndex = indexCounter;
        indexCounter++;

        for (i = 0; i < numberSlices; i++)
        {
            *tp = centerIndex;      ++tp;
            *tp = indexCounter + 1; ++tp;
            *tp = indexCounter;     ++tp;

            indexCounter++;
        }
        indexCounter++;

        // Sides
        for (i = 0; i < numberSlices; i++)
        {
            *tp = indexCounter;     ++tp;
            *tp = indexCounter + 1; ++tp;
            *tp = indexCounter + 2; ++tp;

            *tp = indexCounter + 2; ++tp;
            *tp = indexCounter + 1; ++tp;
            *tp = indexCounter + 3; ++tp;

            indexCounter += 2;
        }
    }

    template<typename T>
    void CreateConeIndices(GeometryCreater *pc,const uint numberSlices,const uint numberStacks)
    {
        IBTypeMap<T> ib_map(pc->GetIBMap());
        T *tp=ib_map;

        // Bottom
        uint centerIndex = 0;
        uint indexCounter = 1;
        uint i,j;

        for (i = 0; i < numberSlices; i++)
        {
            *tp = centerIndex;      ++tp;
            *tp = indexCounter;     ++tp;
            *tp = indexCounter + 1; ++tp;

            indexCounter++;
        }
        indexCounter++;

        // Sides
        for (j = 0; j < numberStacks; ++j)
        {
            for (i = 0; i < numberSlices; ++i)
            {
                *tp = indexCounter;                     ++tp;
                *tp = indexCounter + numberSlices + 1;  ++tp;
                *tp = indexCounter + 1;                 ++tp;

                *tp = indexCounter + 1;                 ++tp;
                *tp = indexCounter + numberSlices + 1;  ++tp;
                *tp = indexCounter + numberSlices + 2;  ++tp;

                indexCounter++;
            }
            indexCounter++;
        }
    }

} // namespace hgl::graph::inline_geometry
