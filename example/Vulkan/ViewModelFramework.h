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

    Matrix4f                    view_model_matrix;

    ControlCamera               camera;
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

        view_model_matrix=identity();

        InitCamera(w,h);
        return(true);
    }

    virtual void InitCamera(int w,int h)
    {     
        math::vec center_point=bounding_box.CenterPoint();
        math::vec max_point=bounding_box.maxPoint;

        max_point.x*=5.0f;
        max_point.y=center_point.y;
        max_point.z=center_point.z;
        
        camera.type=CameraType::Perspective;
        camera.width=w;
        camera.height=h;
        camera.center=center_point;
        camera.eye=max_point;

        camera.Refresh();      //更新矩阵计算
    }

    bool InitCameraUBO(vulkan::DescriptorSets *desc_set,uint world_matrix_bindpoint)
    {
        ubo_world_matrix=db->CreateUBO(sizeof(WorldMatrix),&camera.matrix);

        if(!ubo_world_matrix)
            return(false);

        return desc_set->BindUBO(world_matrix_bindpoint,*ubo_world_matrix);
    }

    void Resize(int w,int h)override
    {
        camera.width=w;
        camera.height=h;
    }

    virtual void Draw()override
    {
        camera.Refresh();                           //更新相机矩阵
        ubo_world_matrix->Write(&camera.matrix);    //写入缓冲区
        
        render_root.RefreshMatrix(&view_model_matrix);
        render_list.Clear();
        render_root.ExpendToList(&render_list);

        BuildCommandBuffer(&render_list);

        VulkanApplicationFramework::Draw();

        if(key_status[kbW])camera.Forward   (move_speed);else
        if(key_status[kbS])camera.Backward  (move_speed);else
        if(key_status[kbA])camera.Left      (move_speed);else
        if(key_status[kbD])camera.Right     (move_speed);else
        if(key_status[kbR])camera.Up        (move_speed);else
        if(key_status[kbF])camera.Down      (move_speed);else

        if(key_status[kbLeft ])view_model_matrix=rotate(hgl_ang2rad(move_speed*10),camera.forward_vector)*view_model_matrix;else
        if(key_status[kbRight])view_model_matrix=rotate(hgl_ang2rad(-move_speed*10),camera.forward_vector)*view_model_matrix;else
            return;
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
        if(!(mouse_key&(mbLeft|mbRight)))return;

        Vector2f gap=mouse_pos-mouse_last_pos;

        if(mouse_key&mbLeft)
        {
            if(gap.x!=0)camera.HorzRotate(-gap.x/10.0f);
            if(gap.y!=0)camera.VertRotate(-gap.y/10.0f);
        }
        else
        {
            if(gap.x!=0)view_model_matrix=rotate(hgl_ang2rad(gap.x),camera.up_vector)*view_model_matrix;
            if(gap.y!=0)view_model_matrix=rotate(hgl_ang2rad(gap.y),camera.right_vector)*view_model_matrix;
        }

        mouse_last_pos=mouse_pos;
    }

    virtual void MouseWheel(int v,int h,uint)
    {
        camera.Distance(1+(v/1000.0f));
    }
};//class CameraAppFramework
