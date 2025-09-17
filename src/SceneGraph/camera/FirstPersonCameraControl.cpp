#include<hgl/graph/camera/FirstPersonCameraControl.h>

namespace hgl::graph
{
    FirstPersonCameraControl::FirstPersonCameraControl(ViewportInfo *v,Camera *c,UBOCameraInfo *ci):CameraControl(v,c,ci)
    {
        target=Vector3f(0.0f);
        up=Vector3f(0,0,1);
        distance_to_target=0.0f;

        // initialize orientation (radians)
        pitch=0;
        yaw  =deg2rad(-90.0f);
        roll =0;

        input_invert_sign.x=-1;
        input_invert_sign.y=1;

        // default: ~0.2 degrees per pixel (previous default was 0.002 radians/pixel)
        rotation_sensitivity_deg_per_pixel = 0.2f; // degrees per pixel

        UpdateCameraVector();
    }

    void FirstPersonCameraControl::SetTarget(const Vector3f &t)
    {
        if(!camera) return;

        // Compute direction to target and initial forward
        Vector3f dir = t - camera->pos;
        float len = length(dir);

        if(len <= 1e-6f)
        {
            // target coincides with camera position
            forward = Vector3f(1,0,0);
            distance_to_target = 0.0f;
        }
        else
        {
            forward = dir / len; // normalized direction
             // compute Euler from forward
             pitch   = asin(forward.z);
             yaw     = atan2(forward.y, forward.x);

             // ensure forward vector matches yaw/pitch
             UpdateCameraVector();

             // scalar distance along forward
             distance_to_target = dot(t - camera->pos, forward);
         }

        right   =normalize(cross(forward,camera->world_up));
        up      =normalize(cross(right,forward));

        camera->view_line=normalize(camera->pos-t);
    }

    void FirstPersonCameraControl::Refresh()
    {
        if(!camera || !camera_info) return;

        target=camera->pos+forward*distance_to_target;

        camera_info->view       =LookAtMatrix(camera->pos,target,camera->world_up);

        RefreshCameraInfo(camera_info,vi,camera);

        if(ubo_camera_info) ubo_camera_info->Update();
    }

    void FirstPersonCameraControl::UpdateCameraVector()
    {
        if(!camera) return;

        // Compute forward vector directly from yaw (azimuth) and pitch (elevation).
        // This avoids depending on external PolarToVector and gives stable behavior near the poles.
        const float cp = cosf(pitch);
        forward.x = cp * cosf(yaw);
        forward.y = cp * sinf(yaw);
        forward.z = sinf(pitch);

        right   =normalize(cross(forward,camera->world_up));
        up      =normalize(cross(right,forward));
    }

    void FirstPersonCameraControl::Rotate(const Vector2f &axis)
    {
        constexpr float top_limit      =deg2rad( 89.0f);
        constexpr float bottom_limit   =deg2rad(-89.0f);

        // axis is mouse delta in pixels (after scaling in caller).
        // rotation_sensitivity_deg_per_pixel is degrees/pixel, convert to radians when applying.
        const float sens_rad = deg2rad(rotation_sensitivity_deg_per_pixel);

        yaw     -= axis.x * input_invert_sign.x * sens_rad;
        pitch   -= axis.y * input_invert_sign.y * sens_rad;

        // Normalize yaw to [-pi,pi] robustly
        const float PI = 3.14159265358979323846f;
        // Use fmod to keep yaw bounded, then shift to [-PI,PI]
        yaw = fmodf(yaw, 2.0f * PI);
        if(yaw > PI) yaw -= 2.0f * PI;
        else if(yaw < -PI) yaw += 2.0f * PI;

        // Clamp pitch to avoid flipping at the poles
        if(pitch>top_limit) pitch=top_limit;
        if(pitch<bottom_limit) pitch=bottom_limit;
        
        UpdateCameraVector();
    }

    // CameraKeyboardControl implementations
    io::EventProcResult CameraKeyboardControl::OnPressed(const io::KeyboardButton &kb)
    {
        if(io::KeyboardStateEvent::OnPressed(kb)==io::EventProcResult::Continue)
            return(io::EventProcResult::Continue);

        if(kb==io::KeyboardButton::Minus    )move_speed*=0.9f;else
        if(kb==io::KeyboardButton::Equals   )move_speed*=1.1f;

        return(io::EventProcResult::Break);
    }

    bool CameraKeyboardControl::Update()
    {
        // allow multiple keys to be processed simultaneously (remove else-if chain)
        if(HasPressed(io::KeyboardButton::W     )) { if(camera) camera->Forward   (move_speed); }
        if(HasPressed(io::KeyboardButton::S     )) { if(camera) camera->Backward  (move_speed); }
        if(HasPressed(io::KeyboardButton::A     )) { if(camera) camera->Left      (move_speed); }
        if(HasPressed(io::KeyboardButton::D     )) { if(camera) camera->Right     (move_speed); }

        return(true);
    }

    // CameraMouseControl implementations
    io::EventProcResult CameraMouseControl::OnPressed(const Vector2i &mouse_coord,io::MouseButton)
    {
        mouse_last_pos=mouse_coord;

        last_time=cur_time;

        return(io::EventProcResult::Break);
    }

    io::EventProcResult CameraMouseControl::OnWheel(const Vector2i &mouse_coord)
    {
        if(mouse_coord.y==0) return(io::EventProcResult::Continue);
        if(camera) camera->Forward(float(mouse_coord.y)/100.0f);

        return(io::EventProcResult::Break);
    }

    io::EventProcResult CameraMouseControl::OnMove(const Vector2i &mouse_coord)
    {
        mouse_pos=mouse_coord;

        bool left =HasPressed(io::MouseButton::Left);
        bool right=HasPressed(io::MouseButton::Right);

        Vector2f pos=mouse_coord;
        Vector2f gap=pos-mouse_last_pos;

        if(left)
        {
            gap/=-5.0f;

            if(camera) camera->Rotate(gap);
        }
        else
        if(right)
        {
            gap/=10.0f;

            if(camera) camera->Move(Vector3f(gap.x,0,gap.y));
        }

        last_time=cur_time;
        mouse_last_pos=pos;

        return(io::EventProcResult::Continue);
    }
} // namespace hgl::graph
