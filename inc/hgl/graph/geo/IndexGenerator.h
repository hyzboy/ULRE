#pragma once

#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/VertexAttribDataAccess.h>

namespace hgl::graph::inline_geometry
{
    /**
     * 索引生成器
     * 提供统一的索引生成接口，避免重复的模板代码
     */
    namespace IndexGenerator
    {
        /**
         * 写入顺序索引（从start开始的count个索引）
         * @param p 索引缓冲指针
         * @param start 起始索引
         * @param count 索引数量
         */
        template<typename T>
        void WriteSequentialIndices(T *p, const T start, const T count)
        {
            for(T i = start; i < start + count; i++)
            {
                *p = i;
                ++p;
            }
        }

        /**
         * 写入圆形索引（扇形）
         * @param ibo 索引缓冲指针
         * @param edge_count 边数
         */
        template<typename T>
        void WriteCircleIndices(T *ibo, const uint edge_count)
        {
            T *p = ibo;

            for(T i = 1; i < edge_count; i++)
            {
                *p = 0;     ++p;
                *p = i + 1; ++p;
                *p = i;     ++p;
            }

            *p = 0;           ++p;
            *p = 1;           ++p;
            *p = edge_count;
        }

        /**
         * 生成球体索引
         * @param pc GeometryCreater指针
         * @param numberParallels 纬线数
         * @param numberSlices 经线数
         */
        template<typename T>
        void CreateSphereIndices(GeometryCreater *pc, uint numberParallels, const uint numberSlices)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp = ib_map;

            for (uint i = 0; i < numberParallels; i++)
            {
                for (uint j = 0; j < numberSlices; j++)
                {
                    *tp =  i      * (numberSlices + 1) + j;       ++tp;
                    *tp = (i + 1) * (numberSlices + 1) + (j + 1); ++tp;
                    *tp = (i + 1) * (numberSlices + 1) + j;       ++tp;

                    *tp =  i      * (numberSlices + 1) + j;       ++tp;
                    *tp =  i      * (numberSlices + 1) + (j + 1); ++tp;
                    *tp = (i + 1) * (numberSlices + 1) + (j + 1); ++tp;
                }
            }
        }

        /**
         * 生成圆环索引
         * @param pc GeometryCreater指针
         * @param numberSlices 环向切片数
         * @param numberStacks 径向切片数
         */
        template<typename T>
        void CreateTorusIndices(GeometryCreater *pc, uint numberSlices, uint numberStacks)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp = ib_map;

            uint sideCount, faceCount;
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

        /**
         * 生成圆柱体索引
         * @param pc GeometryCreater指针
         * @param numberSlices 切片数
         */
        template<typename T>
        void CreateCylinderIndices(GeometryCreater *pc, const uint numberSlices)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp = ib_map;
            uint i;

            T centerIndex = 0;
            T indexCounter = 1;

            // Bottom
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

        /**
         * 生成圆锥体索引
         * @param pc GeometryCreater指针
         * @param numberSlices 切片数
         * @param numberStacks 堆栈数
         */
        template<typename T>
        void CreateConeIndices(GeometryCreater *pc, const uint numberSlices, const uint numberStacks)
        {
            IBTypeMap<T> ib_map(pc->GetIBMap());
            T *tp = ib_map;

            uint centerIndex = 0;
            uint indexCounter = 1;
            uint i, j;

            // Bottom
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
    }
}
