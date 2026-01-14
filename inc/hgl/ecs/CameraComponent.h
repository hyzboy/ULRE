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

        //struct CameraPostFX
        //{
        //    bool  enable_tone_map   = true;
        //    bool  enable_bloom      = false;
        //    float bloom_intensity   = 0.8f;
        //    float bloom_threshold   = 1.0f;
        //    bool  enable_vignette   = false;
        //    float vignette_strength = 0.25f;
        //};

        //struct CameraExposure
        //{
        //    float aperture_f        = 16.0f;    ///< f-number
        //    float shutter_seconds   = 1.0f/60.0f;
        //    float iso               = 100.0f;
        //    float exposure_comp     = 0.0f;     ///< EV offset
        //};

        class CameraComponent : public Component
        {
        private:

            CameraProjection projection   = CameraProjection::Perspective;
            CameraControlMode control_mode= CameraControlMode::None;

            float fov_y_deg   = 60.0f;    ///< perspective vertical fov in degrees
            float ortho_width = 16.0f;
            float ortho_height= 9.0f;    ///< view height for ortho, width
            float near_clip   = 0.1f;
            float far_clip    = 1000.0f;

//            CameraExposure exposure;
//            CameraPostFX   post_fx;

        public:

            CameraComponent(const std::string& name = "Camera") : Component(name) {}
            ~CameraComponent() override = default;

        public:

            void SetProjectionType(const CameraProjection &cp){projection=cp;}
            CameraProjection GetProjectionType()const{return projection;}

            void SetSize(float width,float height)
            {
                ortho_width  = width;
                ortho_height = height;
            }

            void SetFovYDeg (float fov_deg      ){fov_y_deg = fov_deg;}
            void SetNearClip(float near_plane   ){near_clip = near_plane;}
            void SetFarClip (float far_plane    ){far_clip  = far_plane;}

            float GetFovYDeg    () const { return fov_y_deg; }
            float GetOrthoWidth () const { return ortho_width; }
            float GetOrthoHeight() const { return ortho_height; }
            float GetNearClip   () const { return near_clip; }
            float GetFarClip    () const { return far_clip; }

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

            // Derived matrices (built on demand;)
            math::Matrix4f BuildProjectionMatrix() const
            {
                if(projection==CameraProjection::Perspective)
                    return math::PerspectiveMatrix(fov_y_deg, ortho_width/ortho_height, near_clip, far_clip);

                return math::OrthoMatrix(ortho_width, ortho_height, near_clip, far_clip);
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
