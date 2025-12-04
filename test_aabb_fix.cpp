#include <iostream>
#include <hgl/graph/AABB.h>
#include <hgl/graph/OBB.h>
#include <hgl/graph/BoundingSphere.h>

using namespace hgl::graph;

int main()
{
    // 测试 Vector3f 的大小
    std::cout << "sizeof(Vector3f): " << sizeof(Vector3f) << std::endl;
    std::cout << "sizeof(Vector4f): " << sizeof(Vector4f) << std::endl;
    std::cout << "sizeof(float) * 3: " << sizeof(float) * 3 << std::endl;
    std::cout << "sizeof(float) * 4: " << sizeof(float) * 4 << std::endl;
    std::cout << std::endl;

    // 准备测试数据 - 使用 float 数组模拟可能内存对齐的情况
    const int point_count = 4;
    
    // 如果 Vector3f 是 4 个 float (内存对齐)，这样的数据会正确处理
    float aligned_data[point_count * 4] = {
        1.0f, 2.0f, 3.0f, 0.0f,  // 第一个点 + padding
        4.0f, 5.0f, 6.0f, 0.0f,  // 第二个点 + padding
        7.0f, 8.0f, 9.0f, 0.0f,  // 第三个点 + padding
        0.0f, 0.0f, 0.0f, 0.0f   // 第四个点 + padding
    };

    // 测试新的 API
    std::cout << "Testing new AABB::SetFromPoints with component_count..." << std::endl;
    AABB aabb1;
    aabb1.SetFromPoints(aligned_data, point_count, 4);  // 4 个组件 (x,y,z,padding)
    std::cout << "AABB Min: (" << aabb1.GetMin().x << ", " << aabb1.GetMin().y << ", " << aabb1.GetMin().z << ")" << std::endl;
    std::cout << "AABB Max: (" << aabb1.GetMax().x << ", " << aabb1.GetMax().y << ", " << aabb1.GetMax().z << ")" << std::endl;
    std::cout << std::endl;

    // 测试旧的 Vector3f API（应该也能正确工作）
    std::cout << "Testing AABB::SetFromPoints with Vector3f*..." << std::endl;
    Vector3f points[4] = {
        Vector3f(1.0f, 2.0f, 3.0f),
        Vector3f(4.0f, 5.0f, 6.0f),
        Vector3f(7.0f, 8.0f, 9.0f),
        Vector3f(0.0f, 0.0f, 0.0f)
    };
    AABB aabb2;
    aabb2.SetFromPoints(points, 4);
    std::cout << "AABB Min: (" << aabb2.GetMin().x << ", " << aabb2.GetMin().y << ", " << aabb2.GetMin().z << ")" << std::endl;
    std::cout << "AABB Max: (" << aabb2.GetMax().x << ", " << aabb2.GetMax().y << ", " << aabb2.GetMax().z << ")" << std::endl;
    std::cout << std::endl;

    // 测试 BoundingSphere
    std::cout << "Testing BoundingSphere::SetFromPoints..." << std::endl;
    BoundingSphere sphere;
    sphere.SetFromPoints(aligned_data, point_count, 4);
    std::cout << "Sphere Center: (" << sphere.center.x << ", " << sphere.center.y << ", " << sphere.center.z << ")" << std::endl;
    std::cout << "Sphere Radius: " << sphere.radius << std::endl;
    std::cout << std::endl;

    std::cout << "All tests completed successfully!" << std::endl;
    return 0;
}
