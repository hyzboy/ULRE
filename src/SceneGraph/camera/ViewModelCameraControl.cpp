#include<hgl/graph/camera/ViewModelCameraControl.h>

namespace hgl::graph
{
    // All ViewModelCameraControl methods are now inline in the header file

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
