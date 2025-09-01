#ifndef HGL_GRAPH_SPLINE_GLM_INCLUDE
#define HGL_GRAPH_SPLINE_GLM_INCLUDE

#include <hgl/graph/Spline.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace hgl
{
    namespace graph
    {
        // GLM-based spline type definitions for 3D Z-up coordinate system
        using SplineGLM3f = Spline<glm::vec3, float>;
        using SplineGLM2f = Spline<glm::vec2, float>;
        using SplineGLM3d = Spline<glm::vec3, double>;

        /**
         * 3D Spline Curve Generator for Z-up coordinate system
         * 
         * This class provides a convenient interface for creating and evaluating
         * 3D spline curves using GLM vec3 points. The implementation is designed
         * for Z-up coordinate systems commonly used in 3D graphics applications.
         */
        class SplineCurve3D
        {
        private:
            SplineGLM3f spline;
            std::vector<glm::vec3> control_points;

        public:
            /**
             * Constructor
             * @param order Spline order (degree = order - 1), minimum is 2
             * @param node_type Type of nodal vector (UNIFORM or OPEN_UNIFORM)
             */
            SplineCurve3D(int order = 3, SplineNode node_type = SplineNode::OPEN_UNIFORM)
                : spline(order, node_type)
            {
                // Initialize with minimum required points to avoid assertion failures
                control_points.resize(order, glm::vec3(0.0f));
                spline.set_ctrl_points(control_points);
                control_points.clear(); // Clear for user to add actual points
            }

            /**
             * Set control points for the spline curve
             * @param points Vector of 3D control points (glm::vec3)
             */
            void SetControlPoints(const std::vector<glm::vec3>& points)
            {
                control_points = points;
                if (control_points.size() >= static_cast<size_t>(spline.get_order()))
                {
                    spline.set_ctrl_points(points);
                }
            }

            /**
             * Add a single control point to the curve
             * @param point 3D control point to add
             */
            void AddControlPoint(const glm::vec3& point)
            {
                control_points.push_back(point);
                if (control_points.size() >= static_cast<size_t>(spline.get_order()))
                {
                    spline.set_ctrl_points(control_points);
                }
            }

            /**
             * Clear all control points
             */
            void ClearControlPoints()
            {
                control_points.clear();
            }

            /**
             * Get the current control points
             * @return Vector of control points
             */
            const std::vector<glm::vec3>& GetControlPoints() const
            {
                return control_points;
            }

            /**
             * Evaluate the spline at parameter t
             * @param t Parameter value [0.0, 1.0]
             * @return 3D position on the spline curve
             */
            glm::vec3 Evaluate(float t) const
            {
                if (!IsValid())
                    return glm::vec3(0.0f);
                
                return spline.eval_f(t);
            }

            /**
             * Evaluate the spline derivative (tangent) at parameter t
             * @param t Parameter value [0.0, 1.0]
             * @return 3D tangent vector at the specified point
             */
            glm::vec3 EvaluateDerivative(float t) const
            {
                if (!IsValid())
                    return glm::vec3(0.0f, 0.0f, 1.0f); // Default Z-up
                
                return spline.eval_df(t);
            }

            /**
             * Get normalized tangent vector at parameter t
             * @param t Parameter value [0.0, 1.0]
             * @return Normalized tangent vector
             */
            glm::vec3 GetTangent(float t) const
            {
                glm::vec3 derivative = EvaluateDerivative(t);
                float length = glm::length(derivative);
                if (length > 0.0001f)
                    return derivative / length;
                return glm::vec3(0.0f, 0.0f, 1.0f); // Default Z-up
            }

            /**
             * Generate a series of points along the spline
             * @param num_points Number of points to generate
             * @return Vector of interpolated points along the curve
             */
            std::vector<glm::vec3> GeneratePoints(int num_points) const
            {
                std::vector<glm::vec3> points;
                if (num_points <= 0 || !IsValid())
                    return points;

                points.reserve(num_points);
                
                for (int i = 0; i < num_points; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
                    points.push_back(Evaluate(t));
                }
                
                return points;
            }

            /**
             * Generate points with tangent vectors for oriented objects
             * @param num_points Number of points to generate
             * @return Vector of pairs containing position and tangent
             */
            std::vector<std::pair<glm::vec3, glm::vec3>> GeneratePointsWithTangents(int num_points) const
            {
                std::vector<std::pair<glm::vec3, glm::vec3>> result;
                if (num_points <= 0 || !IsValid())
                    return result;

                result.reserve(num_points);
                
                for (int i = 0; i < num_points; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(num_points - 1);
                    result.emplace_back(Evaluate(t), GetTangent(t));
                }
                
                return result;
            }

            /**
             * Calculate approximate arc length of the spline
             * @param num_segments Number of segments for approximation
             * @return Approximate total length of the curve
             */
            float CalculateLength(int num_segments = 100) const
            {
                if (!IsValid() || num_segments <= 0)
                    return 0.0f;

                float total_length = 0.0f;
                glm::vec3 prev_point = Evaluate(0.0f);
                
                for (int i = 1; i <= num_segments; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(num_segments);
                    glm::vec3 current_point = Evaluate(t);
                    total_length += glm::length(current_point - prev_point);
                    prev_point = current_point;
                }
                
                return total_length;
            }

            /**
             * Set the nodal vector type
             * @param type UNIFORM or OPEN_UNIFORM
             */
            void SetNodeType(SplineNode type)
            {
                spline.set_node_type(type);
            }

            /**
             * Get the spline order
             * @return Current spline order
             */
            int GetOrder() const
            {
                return spline.get_order();
            }

            /**
             * Check if the spline has enough control points to be evaluated
             * @return True if the spline can be evaluated
             */
            bool IsValid() const
            {
                return control_points.size() >= static_cast<size_t>(spline.get_order());
            }
        };

        /**
         * Utility functions for Z-up coordinate system splines
         */
        namespace SplineUtils
        {
            /**
             * Create a simple circular spline in the XY plane (Z-up)
             * @param center Center point of the circle
             * @param radius Radius of the circle
             * @param num_points Number of control points
             * @return SplineCurve3D object representing a circle
             */
            inline SplineCurve3D CreateCircle(const glm::vec3& center, float radius, int num_points = 8)
            {
                SplineCurve3D spline(3, SplineNode::OPEN_UNIFORM);
                std::vector<glm::vec3> points;
                
                for (int i = 0; i < num_points; ++i)
                {
                    float angle = 2.0f * M_PI * static_cast<float>(i) / static_cast<float>(num_points);
                    glm::vec3 point = center + glm::vec3(
                        radius * cosf(angle),
                        radius * sinf(angle),
                        0.0f
                    );
                    points.push_back(point);
                }
                
                // Close the circle by adding the first point at the end
                points.push_back(points[0]);
                
                spline.SetControlPoints(points);
                return spline;
            }

            /**
             * Create a helical spline around the Z axis
             * @param center Base center point
             * @param radius Radius of the helix
             * @param height Total height of the helix
             * @param turns Number of complete turns
             * @param points_per_turn Number of control points per turn
             * @return SplineCurve3D object representing a helix
             */
            inline SplineCurve3D CreateHelix(const glm::vec3& center, float radius, float height, 
                                             float turns, int points_per_turn = 8)
            {
                SplineCurve3D spline(3, SplineNode::OPEN_UNIFORM);
                std::vector<glm::vec3> points;
                
                int total_points = static_cast<int>(turns * points_per_turn) + 1;
                
                for (int i = 0; i < total_points; ++i)
                {
                    float t = static_cast<float>(i) / static_cast<float>(total_points - 1);
                    float angle = 2.0f * M_PI * turns * t;
                    float z = height * t;
                    
                    glm::vec3 point = center + glm::vec3(
                        radius * cosf(angle),
                        radius * sinf(angle),
                        z
                    );
                    points.push_back(point);
                }
                
                spline.SetControlPoints(points);
                return spline;
            }
        }
    }//namespace graph
}//namespace hgl

#endif//HGL_GRAPH_SPLINE_GLM_INCLUDE