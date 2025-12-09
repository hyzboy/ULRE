#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateSphere(GeometryCreater *pc,const uint numberSlices)
    {
        uint numberParallels = (numberSlices+1) / 2;
        uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
        uint numberIndices = numberParallels * numberSlices * 6;

        const double angleStep = double(2.0f * HGL_PI) / ((double) numberSlices);

        // used later to help us calculating tangents vectors
        float helpVector[3] = { 1.0f, 0.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];
        float tex_x;

        if(!pc->Init("Sphere",numberVertices,numberIndices))
            return(nullptr);

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

        for (uint i = 0; i < numberParallels + 1; i++)
        {
            for (uint j = 0; j < numberSlices + 1; j++)
            {
                float x = sin(angleStep * (double) i) * sin(angleStep * (double) j);
                float y = sin(angleStep * (double) i) * cos(angleStep * (double) j);
                float z = cos(angleStep * (double) i);

                *vp=x;++vp;
                *vp=y;++vp;
                *vp=z;++vp;

                if(np)
                {
                    *np=x;++np;
                    *np=y;++np;
                    *np=z;++np;
                }

                if(tcp)
                {
                    tex_x=(float) j / (float) numberSlices;

                    *tcp=tex_x;++tcp;
                    *tcp=1.0f - (float) i / (float) numberParallels;++tcp;

                    if(tp)
                    {
                        // use quaternion to get the tangent vector
                        glusQuaternionRotateRyf(helpQuaternion, 360.0f * tex_x);
                        glusQuaternionGetMatrix4x4f(helpMatrix, helpQuaternion);

                        glusMatrix4x4MultiplyVector3f(tp, helpMatrix, helpVector);
                        tp+=3;
                    }
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateSphereIndices<uint16>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U32)CreateSphereIndices<uint32>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U8 )CreateSphereIndices<uint8 >(pc,numberParallels,numberSlices);else
                return(nullptr);
        }

        Geometry *p=pc->Create();

        BoundingVolumes bv;

        bv.SetFromAABB(math::Vector3f(-1.0f,-1.0f,-1.0f),math::Vector3f(1.0f,1.0f,1.0f));

        p->SetBoundingVolumes(bv);

        return p;
    }

    Geometry *CreateDome(GeometryCreater *pc,const uint numberSlices)
    {
        if(!pc)return(nullptr);

        uint numberParallels = numberSlices / 4;
        uint numberVertices = (numberParallels + 1) * (numberSlices + 1);
        uint numberIndices = numberParallels * numberSlices * 6;

        float angleStep = (2.0f * HGL_PI) / ((float) numberSlices);

        // used later to help us calculating tangents vectors
        float helpVector[3] = { 1.0f, 0.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];
        float tex_x;

        if (numberSlices < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;

        if(!pc->Init("Dome",numberVertices,numberIndices))
            return(nullptr);
                
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

        for (uint i = 0; i < numberParallels + 1; i++)
        {
            for (uint j = 0; j < numberSlices + 1; j++)
            {
                uint vertexIndex = (i * (numberSlices + 1) + j) * 4;
                uint normalIndex = (i * (numberSlices + 1) + j) * 3;
                uint tangentIndex = (i * (numberSlices + 1) + j) * 3;
                uint texCoordsIndex = (i * (numberSlices + 1) + j) * 2;
                    
                float x = sin(angleStep * (double) i) * sin(angleStep * (double) j);
                float y = sin(angleStep * (double) i) * cos(angleStep * (double) j);
                float z = cos(angleStep * (double) i);

                *vp=x;++vp;
                *vp=y;++vp;
                *vp=z;++vp;

                if(np)
                {
                    *np=+x;++np;
                    *np=-y;++np;
                    *np=+z;++np;
                }

                if(tcp)
                {                        
                    tex_x=(float) j / (float) numberSlices;

                    *tcp=tex_x;++tcp;
                    *tcp=1.0f - (float) i / (float) numberParallels;++tcp;

                    if(tp)
                    {
                        // use quaternion to get the tangent vector
                        glusQuaternionRotateRyf(helpQuaternion, 360.0f * tex_x);
                        glusQuaternionGetMatrix4x4f(helpMatrix, helpQuaternion);

                        glusMatrix4x4MultiplyVector3f(tp, helpMatrix, helpVector);
                        tp+=3;
                    }
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateSphereIndices<uint16>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U32)CreateSphereIndices<uint32>(pc,numberParallels,numberSlices);else
            if(index_type==IndexType::U8 )CreateSphereIndices<uint8 >(pc,numberParallels,numberSlices);else
                return(nullptr);
        }

        Geometry *p=pc->Create();

        BoundingVolumes bv;

        bv.SetFromAABB(math::Vector3f(-1.0f,-1.0f,-1.0f),math::Vector3f(1.0f,1.0f,1.0f));        //这个不对，待查

        p->SetBoundingVolumes(bv);

        return p;
    }

    Geometry *CreateTorus(GeometryCreater *pc,const TorusCreateInfo *tci)
    {
        if(!pc)return(nullptr);

        // s, t = parametric values of the equations, in the range [0,1]
        float s = 0;
        float t = 0;

        // sIncr, tIncr are increment values aplied to s and t on each loop iteration to generate the torus
        float sIncr;
        float tIncr;

        // to store precomputed sin and cos values
        float cos2PIs, sin2PIs, cos2PIt, sin2PIt;

        uint sideCount,faceCount;

        uint numberVertices;
        uint numberIndices;

        // used later to help us calculating tangents vectors
        float helpVector[3] = { 0.0f, 1.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];

        float torusRadius = (tci->outerRadius - tci->innerRadius) / 2.0f;
        float centerRadius = tci->outerRadius - torusRadius;

        numberVertices = (tci->numberStacks + 1) * (tci->numberSlices + 1);
        numberIndices = tci->numberStacks * tci->numberSlices * 2 * 3; // 2 triangles per face * 3 indices per triangle

        if (tci->numberSlices < 3 || tci->numberStacks < 3 || numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return(nullptr);

        sIncr = 1.0f / (float) tci->numberSlices;
        tIncr = 1.0f / (float) tci->numberStacks;

        if(!pc->Init("Torus",numberVertices,numberIndices))
            return(nullptr);                
                
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

        // generate vertices and its attributes
        for (sideCount = 0; sideCount <= tci->numberSlices; ++sideCount, s += sIncr)
        {
            // precompute some values
            cos2PIs = cos(2.0f * HGL_PI * s);
            sin2PIs = sin(2.0f * HGL_PI * s);

            t = 0.0f;
            for (faceCount = 0; faceCount <= tci->numberStacks; ++faceCount, t += tIncr)
            {
                // precompute some values
                cos2PIt = cos(2.0f * HGL_PI * t);
                sin2PIt = sin(2.0f * HGL_PI * t);

                // generate vertex and stores it in the right position
                *vp = torusRadius * sin2PIt; ++vp;
                *vp =-(centerRadius + torusRadius * cos2PIt) * cos2PIs; ++vp;
                *vp = (centerRadius + torusRadius * cos2PIt) * sin2PIs; ++vp;

                if(np)
                {
                    *np =  sin2PIt;             ++np;
                    *np = -cos2PIs * cos2PIt;   ++np;
                    *np =  sin2PIs * cos2PIt;   ++np;
                }

                if(tcp)
                {
                    *tcp = s*tci->uv_scale.x; ++tcp;
                    *tcp = t*tci->uv_scale.y; ++tcp;
                }

                if(tp)
                {
                    glusQuaternionRotateRzf(helpQuaternion, 360.0f * s);
                    glusQuaternionGetMatrix4x4f(helpMatrix, helpQuaternion);

                    glusMatrix4x4MultiplyVector3f(tp, helpMatrix, helpVector);
                    tp+=3;
                }
            }
        }

        //索引
        {
            const IndexType index_type=pc->GetIndexType();

            if(index_type==IndexType::U16)CreateTorusIndices<uint16>(pc,tci->numberSlices,tci->numberStacks);else
            if(index_type==IndexType::U32)CreateTorusIndices<uint32>(pc,tci->numberSlices,tci->numberStacks);else
            if(index_type==IndexType::U8 )CreateTorusIndices<uint8 >(pc,tci->numberSlices,tci->numberStacks);else
                return(nullptr);
        }

        Geometry *p=pc->Create();

        {
            BoundingVolumes bv;

            float maxExtent = centerRadius + torusRadius;
            float minExtent = centerRadius - torusRadius;

            bv.SetFromAABB(math::Vector3f(-torusRadius, -maxExtent, -maxExtent),
                           Vector3f( torusRadius,  maxExtent,  maxExtent));

            p->SetBoundingVolumes(bv);
        }

        return p;
    }
} // namespace
