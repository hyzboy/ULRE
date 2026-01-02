#include"InlineGeometryCommon.h"
#include<hgl/math/Quaternion.h>
#include<hgl/math/MatrixOperations.h>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateSphere(GeometryCreater *pc,const uint numberSlices)
    {
        uint numberParallels = (numberSlices+1) / 2;
        uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
        uint numberIndices = numberParallels * numberSlices * 6;

        const double angleStep = double(2.0f * std::numbers::pi_v<float>) / ((double) numberSlices);

        // Validate parameters using new validator
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        if(!pc->Init("Sphere",numberVertices,numberIndices))
            return(nullptr);

        // Use new GeometryBuilder for vertex attribute management
        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return(nullptr);

        // For tangent calculation using CMMATH
        const Vector3f helpVector(1.0f, 0.0f, 0.0f);

        for (uint i = 0; i < numberParallels + 1; i++)
        {
            for (uint j = 0; j < numberSlices + 1; j++)
            {
                float x = sin(angleStep * (double) i) * sin(angleStep * (double) j);
                float y = sin(angleStep * (double) i) * cos(angleStep * (double) j);
                float z = cos(angleStep * (double) i);

                float tex_x = (float) j / (float) numberSlices;
                float tex_y = 1.0f - (float) i / (float) numberParallels;

                // Calculate tangent using CMMATH quaternion functions
                if(builder.HasTangents())
                {
                    Quatf quat = RotationQuat(360.0f * tex_x, AxisVector::Y);
                    Matrix4f matrix = ToMatrix(quat);
                    Vector3f tangent = TransformDirection(matrix, helpVector);
                    
                    builder.WriteFullVertex(x, y, z,
                                          x, y, z,  // normal same as position for sphere
                                          tangent.x, tangent.y, tangent.z,
                                          tex_x, tex_y);
                }
                else
                {
                    builder.WriteVertex(x, y, z);
                    builder.WriteNormal(x, y, z);
                    builder.WriteTexCoord(tex_x, tex_y);
                }
            }
        }

        // Generate indices using new IndexGenerator
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)IndexGenerator::CreateSphereIndices<uint16>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U32)IndexGenerator::CreateSphereIndices<uint32>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U8 )IndexGenerator::CreateSphereIndices<uint8 >(pc,numberParallels,numberSlices);else
                return(nullptr);
        }

        return pc->CreateWithAABB(
            math::Vector3f(-1.0f, -1.0f, -1.0f),
            math::Vector3f(1.0f, 1.0f, 1.0f));
    }

    Geometry *CreateDome(GeometryCreater *pc,const uint numberSlices)
    {
        // 1. 参数验证
        if(!pc)return(nullptr);

        // 2. 计算几何体参数
        const uint numberParallels = numberSlices / 4;
        const uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
        const uint numberIndices = numberParallels * numberSlices * 6;
        const float angleStep = (2.0f * std::numbers::pi_v<float>) / ((float) numberSlices);

        // 3. 验证参数
        if(!GeometryValidator::ValidateSlices(numberSlices, 3))
            return nullptr;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        // 4. 初始化
        if(!pc->Init("Dome",numberVertices,numberIndices))
            return(nullptr);

        // 5. 创建 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 6. 预计算常量（用于切线计算的辅助向量）
        const Vector3f helpVector(1.0f, 0.0f, 0.0f);

        // 7. 生成顶点
        for (uint i = 0; i < numberParallels + 1; i++)
        {
            for (uint j = 0; j < numberSlices + 1; j++)
            {
                const float x = sin(angleStep * (double) i) * sin(angleStep * (double) j);
                const float y = sin(angleStep * (double) i) * cos(angleStep * (double) j);
                const float z = cos(angleStep * (double) i);

                // 法线（球面法线）
                const float nx = +x;
                const float ny = -y;
                const float nz = +z;

                // 纹理坐标
                const float tex_x = (float) j / (float) numberSlices;
                const float tex_y = 1.0f - (float) i / (float) numberParallels;

                // 切线（使用四元数计算）
                float tx = 0.0f, ty = 0.0f, tz = 0.0f;
                if(builder.HasTangents())
                {
                    Quatf quat = RotationQuat(360.0f * tex_x, AxisVector::Y);
                    Matrix4f matrix = ToMatrix(quat);
                    Vector3f tangentVec = TransformDirection(matrix, helpVector);
                    tx = tangentVec.x;
                    ty = tangentVec.y;
                    tz = tangentVec.z;
                }

                builder.WriteFullVertex(x, y, z, nx, ny, nz, tx, ty, tz, tex_x, tex_y);
            }
        }

        // 8. 生成索引
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U16)
            IndexGenerator::CreateSphereIndices<uint16>(pc, numberParallels, numberSlices);
        else if(index_type == IndexType::U32)
            IndexGenerator::CreateSphereIndices<uint32>(pc, numberParallels, numberSlices);
        else if(index_type == IndexType::U8)
            IndexGenerator::CreateSphereIndices<uint8>(pc, numberParallels, numberSlices);
        else
            return nullptr;

        // 9. 创建几何体并设置包围体（半球）
        return pc->CreateWithAABB(
            math::Vector3f(-1.0f, -1.0f, 0.0f),
            math::Vector3f(1.0f, 1.0f, 1.0f)
        );
    }

    Geometry *CreateTorus(GeometryCreater *pc, const TorusCreateInfo *tci)
    {
        // 1. 参数验证
        if(!pc || !tci)
            return nullptr;

        // 2. 计算半径
        const float torusRadius = (tci->outerRadius - tci->innerRadius) / 2.0f;
        const float centerRadius = tci->outerRadius - torusRadius;

        // 3. 计算顶点和索引数量
        const uint numberVertices = (tci->numberStacks + 1) * (tci->numberSlices + 1);
        const uint numberIndices = tci->numberStacks * tci->numberSlices * 2 * 3;

        // 4. 验证参数
        if(!GeometryValidator::ValidateSlicesAndStacks(tci->numberSlices, tci->numberStacks, 3, 3))
            return nullptr;

        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;

        // 5. 初始化
        if(!pc->Init("Torus", numberVertices, numberIndices))
            return nullptr;

        // 6. 创建 GeometryBuilder
        GeometryBuilder builder(pc);
        if(!builder.IsValid())
            return nullptr;

        // 7. 预计算常量
        const float sIncr = 1.0f / float(tci->numberSlices);
        const float tIncr = 1.0f / float(tci->numberStacks);
        const Vector3f helpVector(0.0f, 1.0f, 0.0f);

        // 8. 生成顶点
        float s = 0.0f;
        for(uint sideCount = 0; sideCount <= tci->numberSlices; ++sideCount, s += sIncr)
        {
            const float cos2PIs = cos(2.0f * std::numbers::pi_v<float> * s);
            const float sin2PIs = sin(2.0f * std::numbers::pi_v<float> * s);

            float t = 0.0f;
            for(uint faceCount = 0; faceCount <= tci->numberStacks; ++faceCount, t += tIncr)
            {
                const float cos2PIt = cos(2.0f * std::numbers::pi_v<float> * t);
                const float sin2PIt = sin(2.0f * std::numbers::pi_v<float> * t);

                // 位置
                const float px = torusRadius * sin2PIt;
                const float py = -(centerRadius + torusRadius * cos2PIt) * cos2PIs;
                const float pz = (centerRadius + torusRadius * cos2PIt) * sin2PIs;

                // 法线
                const float nx = sin2PIt;
                const float ny = -cos2PIs * cos2PIt;
                const float nz = sin2PIs * cos2PIt;

                // 切线（使用四元数计算）
                float tx = 0.0f, ty = 0.0f, tz = 0.0f;
                if(builder.HasTangents())
                {
                    Quatf quat = RotationQuat(360.0f * s, AxisVector::Z);
                    Matrix4f matrix = ToMatrix(quat);
                    Vector3f tangentVec = TransformDirection(matrix, helpVector);
                    tx = tangentVec.x;
                    ty = tangentVec.y;
                    tz = tangentVec.z;
                }

                // 纹理坐标
                const float u = s * tci->uv_scale.x;
                const float v = t * tci->uv_scale.y;

                builder.WriteFullVertex(px, py, pz, nx, ny, nz, tx, ty, tz, u, v);
            }
        }

        // 9. 生成索引
        const IndexType index_type = pc->GetIndexType();
        if(index_type == IndexType::U16)
            IndexGenerator::CreateTorusIndices<uint16>(pc, tci->numberSlices, tci->numberStacks);
        else if(index_type == IndexType::U32)
            IndexGenerator::CreateTorusIndices<uint32>(pc, tci->numberSlices, tci->numberStacks);
        else if(index_type == IndexType::U8)
            IndexGenerator::CreateTorusIndices<uint8>(pc, tci->numberSlices, tci->numberStacks);
        else
            return nullptr;

        // 10. 创建几何体并设置包围体
        const float maxExtent = centerRadius + torusRadius;
        return pc->CreateWithAABB(
            Vector3f(-torusRadius, -maxExtent, -maxExtent),
            Vector3f(torusRadius, maxExtent, maxExtent)
        );
    }
}//namespace hgl::graph::inline_geometry
