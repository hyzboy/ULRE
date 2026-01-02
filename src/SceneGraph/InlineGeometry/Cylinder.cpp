#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCylinder(GeometryCreater *pc,const CylinderCreateInfo *cci)
    {
        uint numberIndices = cci->numberSlices * 3 * 2 + cci->numberSlices * 6;

        if(numberIndices<=0)
            return(nullptr);

        uint numberVertices = (cci->numberSlices + 2) * 2 + (cci->numberSlices + 1) * 2;

        // Validate parameters using new validator
        if(!GeometryValidator::ValidateBasicParams(pc, numberVertices, numberIndices))
            return nullptr;
            
        if(!GeometryValidator::ValidateSlices(cci->numberSlices))
            return nullptr;

        if(!pc->Init("Cylinder",numberVertices,numberIndices))
            return(nullptr);

        float angleStep = (2.0f * std::numbers::pi_v<float>) / ((float) cci->numberSlices);

        // Use new GeometryBuilder for vertex attribute management
        GeometryBuilder builder(pc);
        
        if(!builder.IsValid())
            return(nullptr);

        // Bottom center vertex
        builder.WriteFullVertex(0.0f, 0.0f, -cci->halfExtend,
                               0.0f, 0.0f, -1.0f,
                               0.0f, 1.0f, 0.0f,
                               0.0f, 0.0f);

        // Bottom ring vertices
        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;
            float cosAngle = cos(currentAngle);
            float sinAngle = sin(currentAngle);

            builder.WriteFullVertex(cosAngle * cci->radius, -sinAngle * cci->radius, -cci->halfExtend,
                                   0.0f, 0.0f, -1.0f,
                                   sinAngle, cosAngle, 0.0f,
                                   0.0f, 0.0f);
        }

        // Top center vertex
        builder.WriteFullVertex(0.0f, 0.0f, cci->halfExtend,
                               0.0f, 0.0f, 1.0f,
                               0.0f, -1.0f, 0.0f,
                               1.0f, 1.0f);

        // Top ring vertices
        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;
            float cosAngle = cos(currentAngle);
            float sinAngle = sin(currentAngle);

            builder.WriteFullVertex(cosAngle * cci->radius, -sinAngle * cci->radius, cci->halfExtend,
                                   0.0f, 0.0f, 1.0f,
                                   -sinAngle, -cosAngle, 0.0f,
                                   1.0f, 1.0f);
        }

        // Side vertices (two rings for proper normals)
        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;
            float cosAngle = cos(currentAngle);
            float sinAngle = sin(currentAngle);

            float sign = -1.0f;

            for (uint j = 0; j < 2; j++)
            {
                builder.WriteFullVertex(cosAngle * cci->radius, -sinAngle * cci->radius, cci->halfExtend * sign,
                                       cosAngle, -sinAngle, 0.0f,
                                       -sinAngle, -cosAngle, 0.0f,
                                       (float)i / (float)cci->numberSlices, (sign + 1.0f) / 2.0f);

                sign = 1.0f;
            }
        }

        // Generate indices using new IndexGenerator
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)IndexGenerator::CreateCylinderIndices<uint16>(pc,cci->numberSlices);else
            if(index_type==IndexType::U32)IndexGenerator::CreateCylinderIndices<uint32>(pc,cci->numberSlices);else
            if(index_type==IndexType::U8 )IndexGenerator::CreateCylinderIndices<uint8 >(pc,cci->numberSlices);else
                return(nullptr);
        }

        Geometry *p=pc->Create();

        BoundingVolumes bv;

        bv.SetFromAABB(math::Vector3f(-cci->radius,-cci->radius,-cci->halfExtend),
                       Vector3f( cci->radius, cci->radius, cci->halfExtend));

        p->SetBoundingVolumes(bv);

        return p;
    }
} // namespace
