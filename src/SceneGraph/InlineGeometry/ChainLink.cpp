#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    // Create a chain link by adapting torus creation but allowing elliptical major circle
    Primitive *CreateChainLink(PrimitiveCreater *pc, const ChainLinkCreateInfo *clci)
    {
        if(!pc||!clci) return nullptr;

        const uint numberSlices = (clci->numberSlices < 3) ? 3 : clci->numberSlices;
        const uint numberStacks = (clci->numberStacks < 3) ? 3 : clci->numberStacks;

        float torusRadius = clci->minor_radius; // cross-section radius
        float centerRadius = clci->major_radius; // center-to-crosscenter

        uint numberVertices = (numberStacks + 1) * (numberSlices + 1);
        uint numberIndices = numberStacks * numberSlices * 2 * 3;

        if(numberVertices > GLUS_MAX_VERTICES || numberIndices > GLUS_MAX_INDICES)
            return nullptr;

        if(!pc->Init("ChainLink", numberVertices, numberIndices))
            return nullptr;

        VABMapFloat vertex   (pc->GetVABMap(VAN::Position),VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord),VF_V2F);

        float *vp=vertex;
        float *np=normal;
        float *tp=tangent;
        float *tcp=tex_coord;

        // param s around major loop (slices), t around minor circle (stacks)
        float sIncr = 1.0f / (float) numberSlices;
        float tIncr = 1.0f / (float) numberStacks;

        float s=0.0f;
        float t=0.0f;

        // help for tangent via quaternion rotation
        float helpVector[3] = { 0.0f, 1.0f, 0.0f };
        float helpQuaternion[4];
        float helpMatrix[16];

        for (uint sideCount = 0; sideCount <= numberSlices; ++sideCount, s += sIncr)
        {
            float cosS = cos(2.0f * HGL_PI * s);
            float sinS = sin(2.0f * HGL_PI * s);

            // elliptical major circle: position center is (0, centerRadius*cosS, centerRadius*sinS) but scaled in Y/Z
            float centerX = 0.0f;
            float centerY = centerRadius * cosS * clci->scale_y;
            float centerZ = centerRadius * sinS * clci->scale_z;

            t = 0.0f;
            for (uint faceCount = 0; faceCount <= numberStacks; ++faceCount, t += tIncr)
            {
                float cosT = cos(2.0f * HGL_PI * t);
                float sinT = sin(2.0f * HGL_PI * t);

                // local circle in plane perpendicular to major circle tangent
                // major tangent direction (dCenter/ds) before scaling
                float tangY = -centerRadius * sin(2.0f * HGL_PI * s);
                float tangZ =  centerRadius * cos(2.0f * HGL_PI * s);
                // but scaling affects tangent; compute and normalize
                Vector3f majorTangent(0.0f, tangY * clci->scale_y, tangZ * clci->scale_z);
                majorTangent = glm::normalize(majorTangent);

                // choose a binormal and normal to form orthonormal frame; because centerX always 0, use X axis as radial
                Vector3f normalDir = glm::normalize(Vector3f(1.0f, 0.0f, 0.0f));
                Vector3f binormal = glm::normalize(glm::cross(majorTangent, normalDir));
                Vector3f tangentDir = glm::normalize(glm::cross(binormal, majorTangent));

                // point on minor circle (in local frame: normalDir * (cosT*torusRadius) + tangentDir * (sinT*torusRadius))
                Vector3f offset = normalDir * (cosT * torusRadius) + tangentDir * (sinT * torusRadius);

                Vector3f pos = Vector3f(centerX, centerY, centerZ) + offset;

                if(vp)
                {
                    *vp++ = pos.x; *vp++ = pos.y; *vp++ = pos.z;
                }

                if(np)
                {
                    // normal is offset normalized in world space
                    Vector3f n = glm::normalize(offset);
                    *np++ = n.x; *np++ = n.y; *np++ = n.z;
                }

                if(tcp)
                {
                    *tcp++ = s * clci->uv_scale.x;
                    *tcp++ = t * clci->uv_scale.y;
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

        // indices: reuse torus indices generator
        const IndexType index_type = pc->GetIndexType();
        if(index_type==IndexType::U16) CreateTorusIndices<uint16>(pc, numberSlices, numberStacks); else
        if(index_type==IndexType::U32) CreateTorusIndices<uint32>(pc, numberSlices, numberStacks); else
        if(index_type==IndexType::U8 ) CreateTorusIndices<uint8 >(pc, numberSlices, numberStacks); else
            return nullptr;

        Primitive *p = pc->Create();
        if(p)
        {
            // bounding box conservatively based on scaled center radius + torusRadius
            float maxY = fabs(centerRadius * clci->scale_y) + torusRadius;
            float maxZ = fabs(centerRadius * clci->scale_z) + torusRadius;
            float maxX = torusRadius;

            AABB aabb;
            aabb.SetMinMax(Vector3f(-maxX, -maxY, -maxZ), Vector3f(maxX, maxY, maxZ));
            p->SetBoundingBox(aabb);
        }

        return p;
    }

} // namespace hgl::graph::inline_geometry
