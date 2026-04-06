#include <hgl/graph/camera/ReversedZProj.h>
#include <cmath>

namespace hgl::graph
{
    Matrix4f MakeInfiniteReversedZProj(float fov_y_radians, float aspect, float near_z)
    {
        const float f = 1.0f / std::tan(fov_y_radians * 0.5f);

        // Reversed-Z infinite far plane projection matrix (Vulkan NDC: depth [0,1])
        // Maps: near → 1.0, ∞ → 0.0
        // Signs match PerspectiveMatrix convention (negative X & Y for engine coordinate system)
        Matrix4f m(0.0f);
        m[0][0] = -f / aspect;
        m[1][1] = -f;
        m[2][2] = 0.0f;
        m[2][3] = -1.0f;
        m[3][2] = near_z;  // near_z (positive)
        return m;
    }
}
