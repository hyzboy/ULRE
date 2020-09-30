#pragma once

#include"VulkanAppFramework.h"

using namespace hgl;
using namespace hgl::graph;

class ViewModelFramework:public VulkanApplicationFramework
{
private:

    vulkan::Buffer *            ubo_world_matrix    =nullptr;

protected:

    AABB                        bounding_box;

    Matrix4f                    object_matrix,origin_matrix;

    ControlCamera               camera,origin_camera;
    float                       move_speed=1;

    Vector2f                    mouse_last_pos;

protected:

    SceneNode                   render_root;
    RenderList                  render_list;

public:

    virtual ~ViewModelFramework()=default;

    virtual bool Init(int w,int h,const AABB &aabb)
    {
        if(!VulkanApplicationFramework::Init(w,h))
            return(false);

        bounding_box=aabb;

        InitCamera(w,h);
        return(true);
    }

    virtual void InitCamera(int w,int h)
    {     
        math::vec center_point  =bounding_box.CenterPoint();
        math::vec min_point     =bounding_box.minPoint;
        math::vec size          =bounding_box.Size();

        math::vec eye(  center_point.x,
                        center_point.y-size.y*4,
                        center_point.z,
                        1.0f);
        
        camera.type     =CameraType::Perspective;
        camera.width    =w;
        camera.height   =h;
        camera.vp_width =w;
        camera.vp_height=h;
        camera.center   =center_point;
        camera.eye      =eye;
        camera.znear    =16;
        camera.zfar     =256;

        camera.Refresh();      //更新矩阵计算

        origin_camera=camera;

        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera.matrix);

        move_speed=length(min_point,center_point)/100.0f;

        object_matrix=Matrix4f::identity;
        origin_matrix=object_matrix;
    }

    vulkan::Buffer *GetCameraMatrixBuffer()
    {
        return ubo_world_matrix;
    }

    void Resize(int w,int h)override
    {
        camera.width=w;
        camera.height=h;
    }

    virtual void Draw()override
    {
        const uint32_t index=AcquireNextImage();

//        camera.Refresh();                           //更新相机矩阵
//        ubo_world_matrix->Write(&camera.matrix);    //写入缓冲区
        
        //render_root.RefreshMatrix(&object_matrix);
        render_list.Clear();
        render_root.ExpendToList(&render_list);

        BuildCommandBuffer(index,&render_list);

        SubmitDraw(index);

        if(key_status[kbEnter])
        {
            origin_camera.width=camera.width;
            origin_camera.height=camera.height;
            camera=origin_camera;
            object_matrix=origin_matrix;
        }
        else
        if(key_status[kbW])camera.Forward   (move_speed);else
        if(key_status[kbS])camera.Backward  (move_speed);else
        if(key_status[kbA])camera.Left      (move_speed);else
        if(key_status[kbD])camera.Right     (move_speed);else
        if(key_status[kbR])camera.Up        (move_speed);else
        if(key_status[kbF])camera.Down      (move_speed);else
        {
            auto axis=normalized(camera.center-camera.eye);
            float rotate_rad=hgl_ang2rad(move_speed)*10;

            if(key_status[kbLeft ])
                object_matrix=rotate(-rotate_rad,camera.forward_vector)*object_matrix;else
            if(key_status[kbRight])
                object_matrix=rotate( rotate_rad,camera.forward_vector)*object_matrix;else

            return;
        }
    }

    virtual void KeyPress(KeyboardButton kb)override
    {
        if(kb==kbMinus)move_speed*=0.9f;else
        if(kb==kbEquals)move_speed*=1.1f;else
            return;
    }

    virtual void MouseDown(uint) override
    {
        mouse_last_pos=mouse_pos;
    }

    virtual void MouseMove() override
    {
        if(!mouse_key)return;

        Vector2f gap=mouse_pos-mouse_last_pos;

        if(mouse_key&mbLeft)
        {
            if(gap.x!=0)camera.HorzRotate(-gap.x/10.0f);
            if(gap.y!=0)camera.VertRotate(-gap.y/10.0f);
        }
        else if(mouse_key&mbRight)
        {
            if(gap.x!=0)object_matrix=rotate(hgl_ang2rad(gap.x),camera.up_vector)*object_matrix;
            if(gap.y!=0)object_matrix=rotate(hgl_ang2rad(gap.y),camera.right_vector)*object_matrix;
        }
        else if(mouse_key&mbMid)
        {
            if(gap.x!=0)camera.Left(gap.x*move_speed/5.0f);
            if(gap.y!=0)camera.Up(gap.y*move_speed/5.0f);
        }

        mouse_last_pos=mouse_pos;
    }

    virtual void MouseWheel(int v,int h,uint)
    {
        camera.Distance(1+(v/1000.0f));
    }
};//class CameraAppFramework
