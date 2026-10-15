#include <hgl/ecs/support/BoundingVolumeCull.h>
#include <hgl/math/geometry/Frustum.h>
#include <hgl/math/Matrix.h>
#include <hgl/math/Projection.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

using namespace hgl;
using namespace hgl::math;
using namespace hgl::ecs;

static int fail_count = 0;

static void Check(bool ok, const char8_t *what)
{
    if (ok)
    {
        GLogInfo(u8"  [PASS] %s", what);
    }
    else
    {
        GLogError(u8"  [FAIL] %s", what);
        ++fail_count;
    }
}

static bool Near(float a, float b, float eps = 1e-3f)
{
    return std::fabs(a - b) <= eps;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestBoundingVolumeCull"));

    GLogInfo(u8"=== Testing Bounding Volume Cull Scale Helper ===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: 世界矩阵中提取最大轴向缩放
    // ─────────────────────────────────────────────────────────────
    {
        GLogInfo(u8"Test 1: GetMaxWorldScale");

        Check(Near(GetMaxWorldScale(glm::mat4(1.0f)), 1.0f),
              u8"单位矩阵 -> 1.0");

        glm::mat4 translate_only = glm::translate(glm::mat4(1.0f), glm::vec3(120.0f, -35.0f, 7.0f));
        Check(Near(GetMaxWorldScale(translate_only), 1.0f),
              u8"纯平移 -> 1.0");

        glm::mat4 uniform = glm::scale(glm::mat4(1.0f), glm::vec3(500.0f));
        Check(Near(GetMaxWorldScale(uniform), 500.0f),
              u8"等比缩放 500 -> 500.0");

        glm::mat4 non_uniform = glm::scale(glm::mat4(1.0f), glm::vec3(1.8f, 1.8f, 16.0f));
        Check(Near(GetMaxWorldScale(non_uniform), 16.0f),
              u8"非等比缩放(1.8,1.8,16) -> 16.0（取最大轴）");

        glm::mat4 rotated = glm::rotate(glm::mat4(1.0f), glm::radians(37.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        Check(Near(GetMaxWorldScale(rotated), 1.0f),
              u8"纯旋转 -> 1.0");

        glm::mat4 rotated_scaled = glm::rotate(glm::mat4(1.0f), glm::radians(37.0f), glm::vec3(0.0f, 1.0f, 0.0f))
                                 * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
        Check(Near(GetMaxWorldScale(rotated_scaled), 0.5f),
              u8"旋转+缩放 0.5 -> 0.5");

        Check(Near(GetMaxWorldScale(glm::mat4(0.0f)), 1.0f),
              u8"退化矩阵 -> 兜底 1.0");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 本地半径 -> 世界半径
    // ─────────────────────────────────────────────────────────────
    {
        GLogInfo(u8"Test 2: ToWorldBoundingRadius");

        // 单位正方形地面的本地包围半径：length((1,1,0)) * 0.5
        const float local_radius = glm::length(glm::vec3(1.0f, 1.0f, 0.0f)) * 0.5f;

        Check(Near(local_radius, 0.7071f, 1e-3f),
              u8"单位正方形本地半径 ≈ 0.7071");

        const glm::mat4 ground_world = glm::scale(glm::mat4(1.0f), glm::vec3(500.0f));

        Check(Near(ToWorldBoundingRadius(local_radius, ground_world), 353.5534f, 1e-2f),
              u8"500 倍缩放地面 -> 世界半径 ≈ 353.55");

        Check(Near(ToWorldBoundingRadius(local_radius, glm::mat4(1.0f)), local_radius),
              u8"无缩放 -> 世界半径等于本地半径");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: 回归测试——大地面不得因半径未缩放而被误剔除
    //
    // 复现 CascadeShadowMap 示例的相机（pos=(0,-28,10)、target=(0,0,3)、
    // FOV 60、aspect 16:9、near 0.1、far 500），对地面中心在漫游路径上的
    // 若干取值做视锥判定：
    //   - 用本地半径（旧行为）必须有被误判为 OUTSIDE 的样本（说明确实会剔除）
    //   - 用世界半径（修复后）必须全部不是 OUTSIDE
    // ─────────────────────────────────────────────────────────────
    {
        GLogInfo(u8"Test 3: 500x ground frustum regression");

        const Vector3f eye(0.0f, -28.0f, 10.0f);
        const Vector3f target(0.0f, 0.0f, 3.0f);

        const Matrix4f view = LookAtMatrix(eye, target, math::AxisVector::Z);
        const Matrix4f proj = PerspectiveMatrixReversedZ(60.0f, 16.0f / 9.0f, 0.1f, 500.0f);
        const Matrix4f vp = proj * view;

        Frustum frustum(vp);

        const float local_radius = glm::length(glm::vec3(1.0f, 1.0f, 0.0f)) * 0.5f;
        const glm::mat4 ground_world = glm::scale(glm::mat4(1.0f), glm::vec3(500.0f));
        const float world_radius = ToWorldBoundingRadius(local_radius, ground_world);

        // 相机附近/身后/远处的若干地面吸附中心（10m 网格）
        const Vector3f centers[] =
        {
            Vector3f(   0.0f, -40.0f, 0.0f),   // 相机身后
            Vector3f(   0.0f, -20.0f, 0.0f),   // 相机正前下方
            Vector3f(   0.0f,   0.0f, 0.0f),   // 视点附近
            Vector3f(   0.0f,  60.0f, 0.0f),   // 前方
            Vector3f( -20.0f,  60.0f, 0.0f),   // 前方偏左
            Vector3f(  30.0f, 120.0f, 0.0f),   // 更远偏右
        };

        int old_rejected = 0;
        int new_rejected = 0;

        for (const Vector3f &c : centers)
        {
            const auto old_scope = frustum.SphereIn(c, local_radius);
            const auto new_scope = frustum.SphereIn(c, world_radius);

            const bool old_out = (old_scope == Frustum::Scope::OUTSIDE);
            const bool new_out = (new_scope == Frustum::Scope::OUTSIDE);

            if (old_out) ++old_rejected;
            if (new_out) ++new_rejected;

            GLogInfo(u8"  center=(%.1f,%.1f,%.1f)  local_r=%.3f scope=%d %s | world_r=%.1f scope=%d %s",
                     c.x, c.y, c.z,
                     local_radius, static_cast<int>(old_scope), old_out ? u8"(剔!)" : u8"",
                     world_radius, static_cast<int>(new_scope), new_out ? u8"(剔!)" : u8"");
        }

        Check(old_rejected > 0,
              u8"旧行为（本地半径）确实会误剔除地面 —— 回归前提成立");

        Check(new_rejected == 0,
              u8"修复后（世界半径）地面全部保留，无一次被剔除");
    }

    if (fail_count > 0)
    {
        GLogError(u8"=== FAILED: %d check(s) ===", fail_count);
        return 1;
    }

    GLogInfo(u8"=== All Checks PASSED ===");
    return 0;
}
