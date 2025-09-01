#include <iostream>
#include <vector>
#include <hgl/graph/SplineGLM.h>

using namespace hgl::graph;

void TestBasicSpline()
{
    std::cout << "Testing basic 3D spline functionality..." << std::endl;
    
    // Create a simple spline curve
    SplineCurve3D curve(3, SplineNode::OPEN_UNIFORM);
    
    // Add some control points for a simple curve in 3D space (Z-up)
    std::vector<glm::vec3> control_points = {
        glm::vec3(0.0f, 0.0f, 0.0f),    // Start
        glm::vec3(1.0f, 0.0f, 1.0f),    // Up and forward
        glm::vec3(2.0f, 1.0f, 2.0f),    // Right, up and forward
        glm::vec3(3.0f, 0.0f, 1.0f),    // Forward and slightly up
        glm::vec3(4.0f, 0.0f, 0.0f)     // End at ground level
    };
    
    curve.SetControlPoints(control_points);
    
    if (!curve.IsValid()) {
        std::cout << "Error: Spline is not valid!" << std::endl;
        return;
    }
    
    std::cout << "Spline order: " << curve.GetOrder() << std::endl;
    std::cout << "Number of control points: " << curve.GetControlPoints().size() << std::endl;
    
    // Generate points along the curve
    auto points = curve.GeneratePoints(10);
    std::cout << "Generated points along the curve:" << std::endl;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        std::cout << "  Point " << i << ": (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;
    }
    
    // Test tangent vectors
    std::cout << "\nTangent vectors at key points:" << std::endl;
    for (int i = 0; i <= 4; ++i) {
        float t = static_cast<float>(i) / 4.0f;
        glm::vec3 pos = curve.Evaluate(t);
        glm::vec3 tangent = curve.GetTangent(t);
        std::cout << "  t=" << t << ": pos(" << pos.x << ", " << pos.y << ", " << pos.z 
                  << ") tangent(" << tangent.x << ", " << tangent.y << ", " << tangent.z << ")" << std::endl;
    }
    
    // Calculate approximate length
    float length = curve.CalculateLength(50);
    std::cout << "\nApproximate curve length: " << length << std::endl;
    
    std::cout << "Basic spline test completed successfully!\n" << std::endl;
}

void TestCircularSpline()
{
    std::cout << "Testing circular spline generation..." << std::endl;
    
    // Create a circle in the XY plane centered at origin with radius 2.0
    auto circle = SplineUtils::CreateCircle(glm::vec3(0.0f, 0.0f, 1.0f), 2.0f, 8);
    
    if (!circle.IsValid()) {
        std::cout << "Error: Circle spline is not valid!" << std::endl;
        return;
    }
    
    // Generate points around the circle
    auto points = circle.GeneratePoints(16);
    std::cout << "Circle points (should form a circle at Z=1.0):" << std::endl;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        float radius_check = sqrt(p.x * p.x + p.y * p.y);
        std::cout << "  Point " << i << ": (" << p.x << ", " << p.y << ", " << p.z 
                  << ") radius=" << radius_check << std::endl;
    }
    
    std::cout << "Circular spline test completed!\n" << std::endl;
}

void TestHelicalSpline()
{
    std::cout << "Testing helical spline generation..." << std::endl;
    
    // Create a helix around Z-axis
    auto helix = SplineUtils::CreateHelix(glm::vec3(0.0f, 0.0f, 0.0f), 1.0f, 5.0f, 2.0f, 6);
    
    if (!helix.IsValid()) {
        std::cout << "Error: Helix spline is not valid!" << std::endl;
        return;
    }
    
    // Generate points along the helix
    auto points = helix.GeneratePoints(20);
    std::cout << "Helix points (2 turns, height 5.0, radius 1.0):" << std::endl;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        float radius_check = sqrt(p.x * p.x + p.y * p.y);
        std::cout << "  Point " << i << ": (" << p.x << ", " << p.y << ", " << p.z 
                  << ") radius=" << radius_check << std::endl;
    }
    
    std::cout << "Helical spline test completed!\n" << std::endl;
}

void TestInteractiveSpline()
{
    std::cout << "Testing interactive spline building..." << std::endl;
    
    SplineCurve3D curve;
    
    // Simulate adding control points one by one
    std::vector<glm::vec3> points_to_add = {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(2.0f, 0.0f, 2.0f),
        glm::vec3(3.0f, -1.0f, 1.0f)
    };
    
    for (const auto& point : points_to_add) {
        curve.AddControlPoint(point);
        std::cout << "Added point (" << point.x << ", " << point.y << ", " << point.z << ")" << std::endl;
        
        if (curve.IsValid()) {
            glm::vec3 midpoint = curve.Evaluate(0.5f);
            std::cout << "  Curve midpoint: (" << midpoint.x << ", " << midpoint.y << ", " << midpoint.z << ")" << std::endl;
        } else {
            std::cout << "  Curve not yet valid (need at least " << curve.GetOrder() << " points)" << std::endl;
        }
    }
    
    std::cout << "Interactive spline test completed!\n" << std::endl;
}

int main()
{
    std::cout << "3D Spline Curve Generator Test\n";
    std::cout << "==============================\n" << std::endl;
    
    try {
        TestBasicSpline();
        TestCircularSpline();
        TestHelicalSpline();
        TestInteractiveSpline();
        
        std::cout << "All tests completed successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "Error during testing: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}