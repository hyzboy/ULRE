#include <iostream>
#include <hgl/graph/SplineGLM.h>

using namespace hgl::graph;

void DemonstrateZUpCoordinateSystem()
{
    std::cout << "=== Z-up Coordinate System Demonstration ===" << std::endl;
    std::cout << "Creating curves that showcase the Z-up coordinate system\n" << std::endl;
    
    // 1. Simple horizontal curve at different Z levels
    std::cout << "1. Horizontal path at Z=2.0 (elevated platform)" << std::endl;
    SplineCurve3D platform_path(3, SplineNode::OPEN_UNIFORM);
    std::vector<glm::vec3> platform_points = {
        glm::vec3(-2.0f, -2.0f, 2.0f),  // Start corner
        glm::vec3( 0.0f, -2.0f, 2.0f),  // Edge
        glm::vec3( 2.0f,  0.0f, 2.0f),  // Corner
        glm::vec3( 0.0f,  2.0f, 2.0f),  // Top
        glm::vec3(-2.0f,  0.0f, 2.0f)   // Return
    };
    platform_path.SetControlPoints(platform_points);
    
    auto platform_curve = platform_path.GeneratePoints(10);
    std::cout << "Platform path points (Z should be constant at 2.0):" << std::endl;
    for (size_t i = 0; i < platform_curve.size(); ++i) {
        const auto& p = platform_curve[i];
        std::cout << "  (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;
    }
    
    // 2. Vertical climbing path
    std::cout << "\n2. Climbing staircase (Z increases with progression)" << std::endl;
    SplineCurve3D staircase(3, SplineNode::OPEN_UNIFORM);
    std::vector<glm::vec3> stair_points = {
        glm::vec3(0.0f, 0.0f, 0.0f),    // Ground level
        glm::vec3(1.0f, 0.0f, 1.0f),    // First step
        glm::vec3(2.0f, 0.0f, 2.0f),    // Second step
        glm::vec3(3.0f, 0.0f, 3.0f),    // Third step
        glm::vec3(4.0f, 0.0f, 4.0f)     // Top platform
    };
    staircase.SetControlPoints(stair_points);
    
    auto stair_curve = staircase.GeneratePoints(8);
    std::cout << "Staircase path (notice Z progression):" << std::endl;
    for (size_t i = 0; i < stair_curve.size(); ++i) {
        const auto& p = stair_curve[i];
        std::cout << "  Step " << i << ": (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;
    }
    
    // 3. Spiral ramp around Z axis
    std::cout << "\n3. Spiral ramp (circular motion with Z elevation)" << std::endl;
    SplineCurve3D spiral(4, SplineNode::OPEN_UNIFORM);
    std::vector<glm::vec3> spiral_points;
    const float radius = 3.0f;
    const int num_turns = 2;
    const int points_per_turn = 4;
    const float height_per_turn = 2.0f;
    
    for (int i = 0; i <= num_turns * points_per_turn; ++i) {
        float angle = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(points_per_turn);
        float z = height_per_turn * static_cast<float>(i) / static_cast<float>(points_per_turn);
        spiral_points.emplace_back(
            radius * cos(angle),
            radius * sin(angle),
            z
        );
    }
    spiral.SetControlPoints(spiral_points);
    
    auto spiral_curve = spiral.GeneratePoints(16);
    std::cout << "Spiral ramp points (circular XY, increasing Z):" << std::endl;
    for (size_t i = 0; i < spiral_curve.size(); ++i) {
        const auto& p = spiral_curve[i];
        float xy_radius = sqrt(p.x * p.x + p.y * p.y);
        std::cout << "  (" << p.x << ", " << p.y << ", " << p.z << ") r=" << xy_radius << std::endl;
    }
    
    std::cout << "\n=== Z-up Demonstration Complete ===\n" << std::endl;
}

void DemonstrateSplineFeatures()
{
    std::cout << "=== Advanced Spline Features ===" << std::endl;
    
    // Create a curved flight path
    SplineCurve3D flight_path(3, SplineNode::OPEN_UNIFORM);
    std::vector<glm::vec3> waypoints = {
        glm::vec3(0.0f,  0.0f, 10.0f),   // Takeoff
        glm::vec3(5.0f,  2.0f, 15.0f),   // Climb
        glm::vec3(10.0f, 0.0f, 20.0f),   // Cruise altitude
        glm::vec3(15.0f, -3.0f, 18.0f),  // Banking turn
        glm::vec3(20.0f, 0.0f, 12.0f),   // Descent
        glm::vec3(25.0f, 0.0f, 5.0f)     // Landing approach
    };
    flight_path.SetControlPoints(waypoints);
    
    std::cout << "Flight path analysis:" << std::endl;
    std::cout << "Spline order: " << flight_path.GetOrder() << std::endl;
    std::cout << "Total waypoints: " << flight_path.GetControlPoints().size() << std::endl;
    std::cout << "Approximate flight distance: " << flight_path.CalculateLength(50) << " units" << std::endl;
    
    // Generate flight path with orientation data
    auto path_with_tangents = flight_path.GeneratePointsWithTangents(10);
    std::cout << "\nFlight path with orientation vectors:" << std::endl;
    for (size_t i = 0; i < path_with_tangents.size(); ++i) {
        const auto& pos = path_with_tangents[i].first;
        const auto& dir = path_with_tangents[i].second;
        
        // Calculate pitch (angle from horizontal plane)
        float pitch = asin(dir.z) * 180.0f / M_PI;
        
        std::cout << "  Waypoint " << i << ": pos(" << pos.x << ", " << pos.y << ", " << pos.z 
                  << ") heading(" << dir.x << ", " << dir.y << ", " << dir.z 
                  << ") pitch=" << pitch << "°" << std::endl;
    }
    
    std::cout << "\n=== Advanced Features Complete ===\n" << std::endl;
}

void DemonstrateUtilityFunctions()
{
    std::cout << "=== Utility Functions Demonstration ===" << std::endl;
    
    // 1. Create a circle using utility function
    std::cout << "1. Creating circular path around a tower at height 50m" << std::endl;
    auto tower_circle = SplineUtils::CreateCircle(
        glm::vec3(0.0f, 0.0f, 50.0f),  // Center at 50m height
        25.0f,                          // 25m radius
        8                               // 8 control points
    );
    
    auto circle_points = tower_circle.GeneratePoints(12);
    std::cout << "Circular patrol path around tower:" << std::endl;
    for (size_t i = 0; i < circle_points.size(); ++i) {
        const auto& p = circle_points[i];
        float radius_check = sqrt(p.x * p.x + p.y * p.y);
        std::cout << "  Point " << i << ": (" << p.x << ", " << p.y << ", " << p.z 
                  << ") r=" << radius_check << std::endl;
    }
    
    // 2. Create a helix using utility function
    std::cout << "\n2. Creating helical elevator shaft" << std::endl;
    auto elevator_helix = SplineUtils::CreateHelix(
        glm::vec3(0.0f, 0.0f, 0.0f),   // Base at ground level
        2.0f,                           // 2m radius
        30.0f,                          // 30m total height
        3.0f,                           // 3 complete turns
        6                               // 6 points per turn
    );
    
    auto helix_points = elevator_helix.GeneratePoints(15);
    std::cout << "Helical elevator path:" << std::endl;
    for (size_t i = 0; i < helix_points.size(); ++i) {
        const auto& p = helix_points[i];
        float radius_check = sqrt(p.x * p.x + p.y * p.y);
        float height_progress = p.z / 30.0f * 100.0f;  // Percentage of total height
        std::cout << "  Floor " << i << ": (" << p.x << ", " << p.y << ", " << p.z 
                  << ") r=" << radius_check << " height=" << height_progress << "%" << std::endl;
    }
    
    std::cout << "\n=== Utility Functions Complete ===\n" << std::endl;
}

void DemonstrateInteractiveBuilding()
{
    std::cout << "=== Interactive Spline Building ===" << std::endl;
    std::cout << "Simulating real-time path creation for a drone mission\n" << std::endl;
    
    SplineCurve3D drone_path(3, SplineNode::OPEN_UNIFORM);
    
    // Simulate adding waypoints during mission planning
    std::vector<std::pair<glm::vec3, std::string>> mission_points = {
        {glm::vec3(0.0f, 0.0f, 0.0f), "Launch pad"},
        {glm::vec3(10.0f, 0.0f, 5.0f), "Initial climb"},
        {glm::vec3(20.0f, 10.0f, 15.0f), "Survey point A"},
        {glm::vec3(30.0f, 20.0f, 15.0f), "Survey point B"},
        {glm::vec3(40.0f, 10.0f, 20.0f), "High altitude transit"},
        {glm::vec3(50.0f, 0.0f, 10.0f), "Descent checkpoint"},
        {glm::vec3(60.0f, 0.0f, 0.0f), "Landing zone"}
    };
    
    for (size_t i = 0; i < mission_points.size(); ++i) {
        const auto& point = mission_points[i].first;
        const auto& description = mission_points[i].second;
        
        drone_path.AddControlPoint(point);
        
        std::cout << "Added waypoint " << (i + 1) << ": " << description 
                  << " at (" << point.x << ", " << point.y << ", " << point.z << ")" << std::endl;
        
        if (drone_path.IsValid()) {
            // Calculate some mission statistics
            float path_length = drone_path.CalculateLength(20);
            glm::vec3 halfway = drone_path.Evaluate(0.5f);
            
            std::cout << "  → Current mission length: " << path_length << " units" << std::endl;
            std::cout << "  → Halfway point: (" << halfway.x << ", " << halfway.y 
                      << ", " << halfway.z << ")" << std::endl;
            
            // Show direction at takeoff
            glm::vec3 takeoff_direction = drone_path.GetTangent(0.0f);
            std::cout << "  → Takeoff direction: (" << takeoff_direction.x << ", " 
                      << takeoff_direction.y << ", " << takeoff_direction.z << ")" << std::endl;
        } else {
            std::cout << "  → Need " << (drone_path.GetOrder() - drone_path.GetControlPoints().size()) 
                      << " more waypoints to activate autopilot" << std::endl;
        }
        std::cout << std::endl;
    }
    
    std::cout << "=== Interactive Building Complete ===\n" << std::endl;
}

int main()
{
    std::cout << "3D Spline Curve Generator - Comprehensive Demo\n";
    std::cout << "===============================================\n" << std::endl;
    
    try {
        DemonstrateZUpCoordinateSystem();
        DemonstrateSplineFeatures();
        DemonstrateUtilityFunctions();
        DemonstrateInteractiveBuilding();
        
        std::cout << "🎉 All demonstrations completed successfully!" << std::endl;
        std::cout << "\nThe 3D Spline Curve Generator is ready for use in your Z-up coordinate system applications!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "❌ Error during demonstration: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}