#include<hgl/graph/camera/ViewModelCameraControl.h>
#include<hgl/graph/RenderContext.h>

namespace hgl::graph
{
    ViewModelCameraControl::ViewModelCameraControl()
    {
        yaw = 0.0f; pitch = 0.0f; distance = 5.0f; target = Vector3f(0,0,0); input_invert_sign = Vector2f(-1,1); rotation_sensitivity_deg_per_pixel = 0.2f; zoom_sensitivity = 0.1f; min_distance = 0.1f; max_distance = 1000.0f; dirty_basis = true; forward=Vector3f(1,0,0); right=Vector3f(0,1,0); up=Vector3f(0,0,1); }

    void ViewModelCameraControl::UpdateCameraVector()
    { dirty_basis = true; }

    static void RecalcBasis(float yaw,float pitch,Vector3f &forward,Vector3f &right,Vector3f &up,const Camera *cam)
    {
        const float cp = cosf(pitch); forward.x = cp * cosf(yaw); forward.y = cp * sinf(yaw); forward.z = sinf(pitch); Vector3f world_up(0,0,1); if(cam) world_up = cam->world_up; right = normalize(cross(forward,world_up)); up = normalize(cross(right,forward)); }

    void ViewModelCameraControl::Refresh(const RenderContext *rc)
    {
        if(!rc) return; Camera *camera = const_cast<Camera*>(rc->GetCamera()); CameraInfo *ci = const_cast<CameraInfo*>(rc->GetCameraInfo()); const ViewportInfo *vi = rc->GetViewportInfo(); if(!camera || !ci || !vi) return; if(dirty_basis){ RecalcBasis(yaw,pitch,forward,right,up,camera); camera->pos = target - forward * distance; dirty_basis = false; } ci->view = LookAtMatrix(camera->pos, target, camera->world_up); RefreshCameraInfo(ci, vi, camera); }

    void ViewModelCameraControl::Rotate(const Vector2f &delta_pixels)
    { const float sens_rad = deg2rad(rotation_sensitivity_deg_per_pixel); yaw -= delta_pixels.x * input_invert_sign.x * sens_rad; pitch -= delta_pixels.y * input_invert_sign.y * sens_rad; const float top = deg2rad(89.0f); const float bottom = deg2rad(-89.0f); if(pitch > top) pitch = top; if(pitch < bottom) pitch = bottom; const float PI = 3.14159265358979323846f; yaw = fmodf(yaw, 2.0f * PI); if(yaw > PI) yaw -= 2.0f * PI; else if(yaw < -PI) yaw += 2.0f * PI; dirty_basis = true; }

    void ViewModelCameraControl::Zoom(float wheel_delta)
    { distance *= std::pow(1.0f + zoom_sensitivity, -wheel_delta); distance = std::clamp(distance, min_distance, max_distance); dirty_basis = true; }

    // Mouse helper implementations
    io::EventProcResult ViewModelMouseControl::OnPressed(const Vector2i &mouse_coord, io::MouseButton mb)
    { mouse_last = Vector2f(mouse_coord); dragging=true; return io::EventProcResult::Break; }
    io::EventProcResult ViewModelMouseControl::OnMove(const Vector2i &mouse_coord)
    { if(!dragging) return io::EventProcResult::Continue; Vector2f pos(mouse_coord); Vector2f delta = pos - mouse_last; mouse_last = pos; if(camera){ if(HasPressed(io::MouseButton::Left)) camera->Rotate(delta);} return io::EventProcResult::Break; }
    io::EventProcResult ViewModelMouseControl::OnWheel(const Vector2i &mouse_coord)
    { if(camera && mouse_coord.y!=0) camera->Zoom(float(mouse_coord.y)); return io::EventProcResult::Break; }
    io::EventProcResult ViewModelMouseControl::OnReleased(const Vector2i &, io::MouseButton)
    { dragging=false; return io::EventProcResult::Break; }
}
