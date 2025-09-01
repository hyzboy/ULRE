#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>

// Minimal GLM-like vec3 for testing (since we can't easily install GLM in this environment)
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    
    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }
    
    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }
    
    Vec3 operator*(float scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    
    Vec3 operator/(float scalar) const {
        return Vec3(x / scalar, y / scalar, z / scalar);
    }
    
    Vec3& operator/=(float scalar) {
        x /= scalar; y /= scalar; z /= scalar;
        return *this;
    }
    
    float length() const {
        return sqrt(x*x + y*y + z*z);
    }
};

// Minimal implementation of the Spline functionality for testing
namespace hgl {
namespace graph {

enum class SplineNode {
    UNIFORM,
    OPEN_UNIFORM
};

template<typename Point_t, typename Real_t>
class Spline {
private:
    SplineNode _node_type;
    int _k;
    std::vector<Point_t> _point;
    std::vector<Point_t> _vec;
    std::vector<Real_t> _node;
    
    void assert_splines() const {
        assert(_k > 1);
        assert((int)_point.size() >= _k);
        assert(_node.size() == (_k + _point.size()));
        assert(_point.size() == (_vec.size() + 1));
    }
    
    void set_nodal_vector() {
        if (_node_type == SplineNode::OPEN_UNIFORM)
            set_node_to_open_uniform();
        else if (_node_type == SplineNode::UNIFORM)
            set_node_to_uniform();
    }
    
    void set_node_to_uniform() {
        const int n = _point.size() - 1;
        _node.resize(_k + n + 1);
        
        Real_t step = (Real_t)1 / (Real_t)(n - _k + 2);
        for (int i = 0; i < (int)_node.size(); ++i) {
            _node[i] = ((Real_t)i) * step - step * (Real_t)(_k - 1);
        }
    }
    
    void set_node_to_open_uniform() {
        _node.resize(_k + _point.size());
        
        int acc = 1;
        for (int i = 0; i < (int)_node.size(); ++i) {
            if (i < _k)
                _node[i] = 0.;
            else if (i >= ((int)_point.size() + 1))
                _node[i] = 1.;
            else {
                _node[i] = (Real_t)acc / (Real_t)(_point.size() + 1 - _k);
                acc++;
            }
        }
    }
    
    Point_t eval(Real_t u, const std::vector<Point_t>& point, int k, const std::vector<Real_t>& node, int off = 0) const {
        assert(k > 1);
        assert((int)point.size() >= k);
        
        int dec = 0;
        while (u > node[dec + k + off])
            dec++;
            
        std::vector<Point_t> p_rec(k, Point_t());
        for (int i = dec, j = 0; i < (dec + k); ++i, ++j)
            p_rec[j] = point[i];
            
        std::vector<Real_t> node_rec(k + k - 2, (Real_t)0);
        for (int i = (dec + 1), j = 0; i < (dec + k + k - 1); ++i, ++j)
            node_rec[j] = node[i + off];
            
        return eval_rec(u, p_rec, k, node_rec);
    }
    
    Point_t eval_rec(Real_t u, std::vector<Point_t> p_in, int k, std::vector<Real_t> node_in) const {
        if (p_in.size() == 1)
            return p_in[0];
            
        std::vector<Point_t> p_out(k - 1, Point_t());
        for (int i = 0; i < (k - 1); ++i) {
            const Real_t n0 = node_in[i + k - 1];
            const Real_t n1 = node_in[i];
            const Real_t f0 = (n0 - u) / (n0 - n1);
            const Real_t f1 = (u - n1) / (n0 - n1);
            
            p_out[i] = p_in[i] * f0 + p_in[i + 1] * f1;
        }
        
        std::vector<Real_t> node_out(node_in.size() - 2);
        for (int i = 1, j = 0; i < ((int)node_in.size() - 1); ++i, ++j)
            node_out[j] = node_in[i];
            
        return eval_rec(u, p_out, (k - 1), node_out);
    }
    
public:
    Spline(int k = 2, SplineNode node_type = SplineNode::OPEN_UNIFORM)
        : _node_type(node_type), _k(k), _point(_k), _vec(_k - 1), _node(_k + _point.size()) {
        assert_splines();
    }
    
    void set_ctrl_points(const std::vector<Point_t>& point) {
        _point = point;
        _vec.resize(_point.size() - 1);
        for (int i = 0; i < (int)_vec.size(); ++i)
            _vec[i] = _point[i + 1] - _point[i];
        set_nodal_vector();
        assert_splines();
        
        for (int i = 0; i < (int)_vec.size(); ++i)
            _vec[i] /= _node[_k + i] - _node[i + 1];
    }
    
    Point_t eval_f(Real_t u) const {
        u = std::max(std::min(u, (Real_t)1), (Real_t)0);
        return eval(u, _point, _k, _node);
    }
    
    Point_t eval_df(Real_t u) const {
        u = std::max(std::min(u, (Real_t)1), (Real_t)0);
        return eval(u, _vec, (_k - 1), _node, 1) * (Real_t)(_k - 1);
    }
    
    int get_order() const { return _k; }
};

class SplineCurve3D {
private:
    Spline<Vec3, float> spline;
    std::vector<Vec3> control_points;
    
public:
    SplineCurve3D(int order = 3, SplineNode node_type = SplineNode::OPEN_UNIFORM)
        : spline(order, node_type) {}
        
    void SetControlPoints(const std::vector<Vec3>& points) {
        control_points = points;
        spline.set_ctrl_points(points);
    }
    
    void AddControlPoint(const Vec3& point) {
        control_points.push_back(point);
        spline.set_ctrl_points(control_points);
    }
    
    Vec3 Evaluate(float t) const {
        if (control_points.size() < 2)
            return Vec3(0.0f, 0.0f, 0.0f);
        return spline.eval_f(t);
    }
    
    Vec3 GetTangent(float t) const {
        if (control_points.size() < 2)
            return Vec3(0.0f, 0.0f, 1.0f);
        Vec3 derivative = spline.eval_df(t);
        float length = derivative.length();
        if (length > 0.0001f)
            return derivative / length;
        return Vec3(0.0f, 0.0f, 1.0f);
    }
    
    std::vector<Vec3> GeneratePoints(int num_points) const {
        std::vector<Vec3> points;
        if (num_points <= 0 || control_points.size() < 2)
            return points;
            
        points.reserve(num_points);
        for (int i = 0; i < num_points; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
            points.push_back(Evaluate(t));
        }
        return points;
    }
    
    bool IsValid() const {
        return control_points.size() >= static_cast<size_t>(spline.get_order());
    }
    
    int GetOrder() const { return spline.get_order(); }
    
    const std::vector<Vec3>& GetControlPoints() const { return control_points; }
};

}} // namespace hgl::graph

using namespace hgl::graph;

void TestBasicSpline()
{
    std::cout << "Testing basic 3D spline functionality..." << std::endl;
    
    SplineCurve3D curve(3, SplineNode::OPEN_UNIFORM);
    
    std::vector<Vec3> control_points = {
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 0.0f, 1.0f),
        Vec3(2.0f, 1.0f, 2.0f),
        Vec3(3.0f, 0.0f, 1.0f),
        Vec3(4.0f, 0.0f, 0.0f)
    };
    
    curve.SetControlPoints(control_points);
    
    if (!curve.IsValid()) {
        std::cout << "Error: Spline is not valid!" << std::endl;
        return;
    }
    
    std::cout << "Spline order: " << curve.GetOrder() << std::endl;
    std::cout << "Number of control points: " << curve.GetControlPoints().size() << std::endl;
    
    auto points = curve.GeneratePoints(10);
    std::cout << "Generated points along the curve:" << std::endl;
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& p = points[i];
        std::cout << "  Point " << i << ": (" << p.x << ", " << p.y << ", " << p.z << ")" << std::endl;
    }
    
    std::cout << "\nTangent vectors at key points:" << std::endl;
    for (int i = 0; i <= 4; ++i) {
        float t = static_cast<float>(i) / 4.0f;
        Vec3 pos = curve.Evaluate(t);
        Vec3 tangent = curve.GetTangent(t);
        std::cout << "  t=" << t << ": pos(" << pos.x << ", " << pos.y << ", " << pos.z 
                  << ") tangent(" << tangent.x << ", " << tangent.y << ", " << tangent.z << ")" << std::endl;
    }
    
    std::cout << "Basic spline test completed successfully!\n" << std::endl;
}

void TestInteractiveSpline()
{
    std::cout << "Testing interactive spline building..." << std::endl;
    
    SplineCurve3D curve;
    
    std::vector<Vec3> points_to_add = {
        Vec3(0.0f, 0.0f, 0.0f),
        Vec3(1.0f, 1.0f, 1.0f),
        Vec3(2.0f, 0.0f, 2.0f),
        Vec3(3.0f, -1.0f, 1.0f)
    };
    
    for (const auto& point : points_to_add) {
        curve.AddControlPoint(point);
        std::cout << "Added point (" << point.x << ", " << point.y << ", " << point.z << ")" << std::endl;
        
        if (curve.IsValid()) {
            Vec3 midpoint = curve.Evaluate(0.5f);
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
        TestInteractiveSpline();
        
        std::cout << "All tests completed successfully!" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "Error during testing: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}