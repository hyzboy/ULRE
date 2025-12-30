#include<hgl/graph/geo/GeometryMath.h>
#include<cmath>

namespace hgl::graph::inline_geometry
{
    namespace GeometryMath
    {
        Vector2f Normalize(const Vector2f &v)
        {
            float len = sqrt(v.x * v.x + v.y * v.y);
            
            if(len <= 1e-8f) 
                return Vector2f(0, 0);
                
            return Vector2f(v.x / len, v.y / len);
        }

        Vector3f Normalize(const Vector3f &v)
        {
            float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            
            if(len <= 1e-8f) 
                return Vector3f(0, 0, 0);
                
            return Vector3f(v.x / len, v.y / len, v.z / len);
        }

        Vector3f TriangleNormal(const Vector3f &A, const Vector3f &B, const Vector3f &C)
        {
            Vector3f AB(B.x - A.x, B.y - A.y, B.z - A.z);
            Vector3f AC(C.x - A.x, C.y - A.y, C.z - A.z);
            
            return Vector3f(AB.y * AC.z - AB.z * AC.y,
                            AB.z * AC.x - AB.x * AC.z,
                            AB.x * AC.y - AB.y * AC.x);
        }

        bool LineIntersect(const Vector2f &p, const Vector2f &r, 
                          const Vector2f &q, const Vector2f &s, 
                          float &t, float &u)
        {
            float rxs = r.x * s.y - r.y * s.x;
            
            if(fabs(rxs) < 1e-8f) 
                return false;
                
            Vector2f qp = Vector2f(q.x - p.x, q.y - p.y);
            t = (qp.x * s.y - qp.y * s.x) / rxs;
            u = (qp.x * r.y - qp.y * r.x) / rxs;
            
            return true;
        }

        float AngleBetween(const float ax, const float ay, const float bx, const float by)
        {
            float la = sqrt(ax*ax + ay*ay);
            float lb = sqrt(bx*bx + by*by);
            
            if(la < 1e-6f || lb < 1e-6f) 
                return 0.0f;
                
            float dot = (ax*bx + ay*by) / (la*lb);
            dot = std::clamp(dot, -1.0f, 1.0f);
            
            return acosf(dot) * 180.0f / 3.14159265358979323846f;
        }

        void QuaternionRotateY(float quaternion[4], const float angle)
        {
            float halfAngleRadian = deg2rad(angle) * 0.5f;

            quaternion[0] = 0.0f;
            quaternion[1] = sin(halfAngleRadian);
            quaternion[2] = 0.0f;
            quaternion[3] = cos(halfAngleRadian);
        }

        void QuaternionRotateZ(float quaternion[4], const float angle)
        {
            float halfAngleRadian = deg2rad(angle) * 0.5f;

            quaternion[0] = 0.0f;
            quaternion[1] = 0.0f;
            quaternion[2] = sin(halfAngleRadian);
            quaternion[3] = cos(halfAngleRadian);
        }

        void QuaternionToMatrix(float matrix[16], const float quaternion[4])
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

        void MatrixMultiplyVector3(float result[3], const float matrix[16], const float vector[3])
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
    }
}
