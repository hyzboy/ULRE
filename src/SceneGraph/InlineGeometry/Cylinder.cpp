#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Geometry *CreateCylinder(GeometryCreater *pc,const CylinderCreateInfo *cci)
    {
        uint numberIndices = cci->numberSlices * 3 * 2 + cci->numberSlices * 6;

        if(numberIndices<=0)
            return(nullptr);

        uint numberVertices = (cci->numberSlices + 2) * 2 + (cci->numberSlices + 1) * 2;

        if(!pc->Init("Cylinder",numberVertices,numberIndices))
            return(nullptr);

        float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

        if (cci->numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;
                
        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        if(!vp)
            return(nullptr);

        *vp =  0.0f;             ++vp;
        *vp =  0.0f;             ++vp;
        *vp = -cci->halfExtend;  ++vp;

        if(np)
        {
            *np = 0.0f; ++np;
            *np = 0.0f; ++np;
            *np =-1.0f; ++np;
        }

        if(tp)
        {
            *tp = 0.0f; ++tp;
            *tp = 1.0f; ++tp;
            *tp = 0.0f; ++tp;
        }

        if(tcp)
        {
            *tcp = 0.0f; ++tcp;
            *tcp = 0.0f; ++tcp;
        }

        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            *vp =  cos(currentAngle) * cci->radius; ++vp;
            *vp = -sin(currentAngle) * cci->radius; ++vp;
            *vp = -cci->halfExtend;                 ++vp;

            if(np)
            {
                *np = 0.0f; ++np;
                *np = 0.0f; ++np;
                *np =-1.0f; ++np;
            }

            if(tp)
            {
                *tp = sin(currentAngle);    ++tp;
                *tp = cos(currentAngle);    ++tp;
                *tp = 0.0f;                 ++tp;
            }

            if(tcp)
            {
                *tcp = 0.0f; ++tcp;
                *tcp = 0.0f; ++tcp;
            }
        }

        *vp = 0.0f;             ++vp;
        *vp = 0.0f;             ++vp;
        *vp = cci->halfExtend;  ++vp;

        if(np)
        {
            *np = 0.0f;    ++np;
            *np = 0.0f;    ++np;
            *np = 1.0f;    ++np;
        }

        if(tp)
        {
            *tp =  0.0f; ++tp;
            *tp = -1.0f; ++tp;
            *tp =  0.0f; ++tp;
        }

        if(tcp)
        {
            *tcp = 1.0f; ++tcp;
            *tcp = 1.0f; ++tcp;
        }

        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            *vp =  cos(currentAngle) * cci->radius; ++vp;
            *vp = -sin(currentAngle) * cci->radius; ++vp;
            *vp =  cci->halfExtend;                 ++vp;

            if(np)
            {
                *np = 0.0f;    ++np;
                *np = 0.0f;    ++np;
                *np = 1.0f;    ++np;
            }

            if(tp)
            {
                *tp = -sin(currentAngle);   ++tp;
                *tp = -cos(currentAngle);   ++tp;
                *tp = 0.0f;                 ++tp;
            }

            if(tcp)
            {
                *tcp = 1.0f; ++tcp;
                *tcp = 1.0f; ++tcp;
            }
        }

        for(uint i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            float sign = -1.0f;

            for (uint j = 0; j < 2; j++)
            {
                *vp =  cos(currentAngle) * cci->radius;     ++vp;
                *vp = -sin(currentAngle) * cci->radius;     ++vp;
                *vp =  cci->halfExtend * sign;              ++vp;

                if(np)
                {
                    *np =  cos(currentAngle);   ++np;
                    *np = -sin(currentAngle);   ++np;
                    *np = 0.0f;                 ++np;
                }

                if(tp)
                {
                    *tp = -sin(currentAngle);   ++tp;
                    *tp = -cos(currentAngle);   ++tp;
                    *tp = 0.0f;                 ++tp;
                }

                if(tcp)
                {
                    *tcp = (float)i / (float)cci->numberSlices;  ++tcp;
                    *tcp = (sign + 1.0f) / 2.0f;                 ++tcp;
                }

                sign = 1.0f;
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateCylinderIndices<uint16>(pc,cci->numberSlices);else
            if(index_type==IndexType::U32)CreateCylinderIndices<uint32>(pc,cci->numberSlices);else
            if(index_type==IndexType::U8 )CreateCylinderIndices<uint8 >(pc,cci->numberSlices);else
                return(nullptr);
        }

        Geometry *p=pc->Create();

        p->SetBoundingBox(  Vector3f(-cci->radius,-cci->radius,-cci->halfExtend),
                            Vector3f( cci->radius, cci->radius, cci->halfExtend));

        return p;
    }
} // namespace
