#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    Primitive *CreateCone(PrimitiveCreater *pc,const ConeCreateInfo *cci)
    {
        uint i, j;

        uint numberVertices = (cci->numberSlices + 2) + (cci->numberSlices + 1) * (cci->numberStacks + 1);
        uint numberIndices = cci->numberSlices * 3 + cci->numberSlices * 6 * cci->numberStacks;

        if(!pc->Init("Cone",numberVertices,numberIndices))
            return(nullptr);

        float angleStep = (2.0f * HGL_PI) / ((float) cci->numberSlices);

        float h = 2.0f * cci->halfExtend;
        float r = cci->radius;
        float l = sqrtf(h*h + r*r);

        if (cci->numberSlices < 3 || cci->numberStacks < 1 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
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

        *vp =  0.0f;            ++vp;
        *vp =  0.0f;            ++vp;
        *vp = -cci->halfExtend; ++vp;

        if(np)
        {
            *np = 0.0f;++np;
            *np = 0.0f;++np;
            *np =-1.0f;++np;
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

        for (i = 0; i < cci->numberSlices + 1; i++)
        {
            float currentAngle = angleStep * (float)i;

            *vp =  cos(currentAngle) * cci->radius; ++vp;
            *vp = -sin(currentAngle) * cci->radius; ++vp;
            *vp = -cci->halfExtend;                 ++vp;

            if(np)
            {
                *np = 0.0f;++np;
                *np = 0.0f;++np;
                *np =-1.0f;++np;
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

        for (j = 0; j < cci->numberStacks + 1; j++)
        {
            float level = (float)j / (float)cci->numberStacks;

            for (i = 0; i < cci->numberSlices + 1; i++)
            {
                float currentAngle = angleStep * (float)i;

                *vp =  cos(currentAngle) * cci->radius * (1.0f - level);    ++vp;
                *vp = -sin(currentAngle) * cci->radius * (1.0f - level);    ++vp;
                *vp = -cci->halfExtend + 2.0f * cci->halfExtend * level;    ++vp;

                if(np)
                {
                    *np = h / l *  cos(currentAngle);   ++np;
                    *np =-h / l *  sin(currentAngle);   ++np;
                    *np = r / l;                        ++np;
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
                    *tcp = level;                                ++tcp;
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateConeIndices<uint16>(pc,cci->numberSlices,cci->numberStacks);else
            if(index_type==IndexType::U32)CreateConeIndices<uint32>(pc,cci->numberSlices,cci->numberStacks);else
            if(index_type==IndexType::U8 )CreateConeIndices<uint8 >(pc,cci->numberSlices,cci->numberStacks);else
                return(nullptr);
        }

        Primitive *p=pc->Create();

        p->SetBoundingBox(Vector3f(-cci->radius,-cci->radius,-cci->halfExtend),
                            Vector3f( cci->radius, cci->radius, cci->halfExtend));

        return p;
    }
} // namespace
