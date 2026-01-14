#pragma once

#include<hgl/ecs/Component.h>
#include<hgl/math/Projection.h>

namespace hgl
{
    namespace ecs
    {
        enum class CameraProjection
        {
            Perspective,
            Orthographic
        };

        enum class CameraControlMode
        {
            None,
            FPS,
            Orbit,
            ViewModel,
            FreeFly
        };

        struct CameraPostFX
        {
            bool  enable_tone_map   = true;
            bool  enable_bloom      = false;
            float bloom_intensity   = 0.8f;
            float bloom_threshold   = 1.0f;
            bool  enable_vignette   = false;
            float vignette_strength = 0.25f;
        };

        struct CameraExposure
        {
            float aperture_f        = 16.0f;    ///< f-number
            float shutter_seconds   = 1.0f/60.0f;
            float iso               = 100.0f;
            float exposure_comp     = 0.0f;     ///< EV offset
        };

        class CameraComponent : public Component
        {
        private:

            CameraProjection projection   = CameraProjection::Perspective;
            CameraControlMode control_mode= CameraControlMode::None;

            float fov_y_deg   = 60.0f;    ///< perspective vertical fov in degrees
            float ortho_height= 10.0f;    ///< view height for ortho, width derived by aspect
            float near_clip   = 0.1f;
            float far_clip    = 1000.0f;

//            CameraExposure exposure;
//            CameraPostFX   post_fx;

        public:

            CameraComponent(const std::string& name = "Camera") : Component(name) {}
            ~CameraComponent() override = default;

        public:

            // Projection setup
            void SetPerspective(float fov_deg, float near_plane, float far_plane)
            {
                projection = CameraProjection::Perspective;
                fov_y_deg  = fov_deg;
                near_clip  = near_plane;
                far_clip   = far_plane;
            }

            void SetOrthographic(float height, float near_plane, float far_plane)
            {
                projection   = CameraProjection::Orthographic;
                ortho_height = height;
                near_clip    = near_plane;
                far_clip     = far_plane;
            }

            CameraProjection GetProjectionType() const { return projection; }
            float GetFovYDeg() const { return fov_y_deg; }
            float GetOrthoHeight() const { return ortho_height; }
            float GetNearClip() const { return near_clip; }
            float GetFarClip() const { return far_clip; }

        public:

            // Control mode (consumed by control system/strategy)
            void SetControlMode(CameraControlMode mode){ control_mode = mode; }
            CameraControlMode GetControlMode() const { return control_mode; }

        public:

            // // Exposure / post FX
            // CameraExposure& GetExposure() { return exposure; }
            // const CameraExposure& GetExposure() const { return exposure; }

            // CameraPostFX& GetPostFX() { return post_fx; }
            // const CameraPostFX& GetPostFX() const { return post_fx; }

        public:

            // Derived matrices (built on demand; aspect provided by caller)
            math::Matrix4f BuildProjectionMatrix(float aspect) const
            {
                if(projection==CameraProjection::Perspective)
                    return math::PerspectiveMatrix(fov_y_deg, aspect, near_clip, far_clip);

                const float width = ortho_height*aspect;
                return math::OrthoMatrix(width, ortho_height, near_clip, far_clip);
            }

            // LookAt 视图矩阵（常用于相机控制系统）
            math::Matrix4f BuildViewMatrixLookAt(const math::Vector3f& eye,
                                                 const math::Vector3f& target,
                                                 const math::Vector3f& up = math::AxisVector::Z) const
            {
                return math::LookAtMatrix(eye, target, up);
            }
        };
    }//namespace ecs
}//namespace hgl
