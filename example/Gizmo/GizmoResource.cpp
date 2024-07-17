﻿#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKRenderResource.h>
#include<hgl/color/Color.h>
#include<hgl/graph/InlineGeometry.h>
#include"GizmoResource.h"

VK_NAMESPACE_BEGIN

namespace
{
    bool InitMI(GizmoResource *gr)
    {
        if(!gr||!gr->mtl)
            return(false);

        Color4f color;

        for(uint i=0;i<uint(GizmoColor::RANGE_SIZE);i++)
        {
            color=GetColor4f(gizmo_color[i],1.0);

            gr->mi[i]=gizmo_rr->CreateMaterialInstance(gr->mtl,nullptr,&color);
            if(!gr->mi[i])
                return(false);
        }

        return(true);
    }

    bool InitGizmoResource2D(GPUDevice *device)
    {
        if(!gizmo_rr)
            return(false);

        RenderPass *render_pass=device->GetRenderPass();
        
        {
            mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"VertexLuminance3D",Prim::Lines);

            cfg.mtl_name="VertexLuminance3D";       //注意必须用不同名字，未来改名材质文件名+cfg hash名
            cfg.position_format=VAT_VEC3;

            mtl::MaterialCreateInfo *mci=CreateVertexLuminance3D(&cfg);

            if(!mci)
                return(false);

            gizmo_line.mtl=gizmo_rr->CreateMaterial(mci);
            if(!gizmo_line.mtl)
                return(false);
        }

        {
            gizmo_line.pipeline=render_pass->CreatePipeline(gizmo_line.mtl,InlinePipeline::Solid3D,Prim::Lines);

            if(!gizmo_line.pipeline)
                return(false);
        }

        if(!InitMI(&gizmo_line))
            return(false);
        
        {
            gizmo_line.vdm=new VertexDataManager(device,gizmo_line.mtl->GetDefaultVIL());

            if(!gizmo_line.vdm)
                return(false);

            if(!gizmo_line.vdm->Init(  HGL_SIZE_1MB,       //最大顶点数量
                                    HGL_SIZE_1MB,       //最大索引数量
                                    IndexType::U16))    //索引类型
                return(false);
        }

        {
        }

        return(true);
    }

    bool InitGizmoResource3D(GPUDevice *device)
    {
        if(!gizmo_rr)
            return(false);

        RenderPass *render_pass=device->GetRenderPass();

        {
            mtl::Material3DCreateConfig cfg(device->GetDeviceAttribute(),"Gizmo3D",Prim::Triangles);

            cfg.local_to_world=true;
            cfg.material_instance=true;

            mtl::MaterialCreateInfo *mci=CreateMaterialGizmo3D(&cfg);

            if(!mci)
                return(false);

            gizmo_triangle.mtl=gizmo_rr->CreateMaterial(mci);
            if(!gizmo_triangle.mtl)
                return(false);
        }

        {
            gizmo_triangle.pipeline=render_pass->CreatePipeline(gizmo_triangle.mtl,InlinePipeline::Solid3D,Prim::Triangles);
            if(!gizmo_triangle.pipeline)
                return(false);
        }

        if(!InitMI(&gizmo_triangle))
            return(false);

        {
            gizmo_triangle.vdm=new VertexDataManager(device,gizmo_triangle.mtl->GetDefaultVIL());

            if(!gizmo_triangle.vdm)
                return(false);

            if(!gizmo_triangle.vdm->Init(  HGL_SIZE_1MB,       //最大顶点数量
                                        HGL_SIZE_1MB,       //最大索引数量
                                        IndexType::U16))    //索引类型
                return(false);
        }

        {
            gizmo_triangle.prim_creater=new PrimitiveCreater(gizmo_triangle.vdm);

            if(!gizmo_triangle.prim_creater)
                return(false);
        }

        {
            using namespace inline_geometry;

            {
                gizmo_prim[size_t(GizmoShape::Plane)]=CreatePlane(gizmo_triangle.prim_creater);
            }

            {
                CubeCreateInfo cci;

                cci.normal=true;
                cci.tangent=false;
                cci.tex_coord=false;

                gizmo_prim[size_t(GizmoShape::Cube)]=CreateCube(gizmo_triangle.prim_creater,&cci);
            }

            {
                gizmo_prim[size_t(GizmoShape::Sphere)]=CreateSphere(gizmo_triangle.prim_creater,8);
            }

            {
                ConeCreateInfo cci;

                cci.radius      =1;         //圆锥半径
                cci.halfExtend  =1;         //圆锤一半高度
                cci.numberSlices=8;         //圆锥底部分割数
                cci.numberStacks=1;         //圆锥高度分割数

                gizmo_prim[size_t(GizmoShape::Cone)]=CreateCone(gizmo_triangle.prim_creater,&cci);
            }

            {
                struct CylinderCreateInfo cci;

                cci.halfExtend  =1;         //圆柱一半高度
                cci.numberSlices=8;         //圆柱底部分割数
                cci.radius      =1;         //圆柱半径

                gizmo_prim[size_t(GizmoShape::Cylinder)]=CreateCylinder(gizmo_triangle.prim_creater,&cci);
            }

            ENUM_CLASS_FOR(GizmoShape,int,i)
            {
                if(!gizmo_prim[i])
                    return(false);
            }
        }

        return(true);
    }
}//namespace

bool InitGizmoResource(GPUDevice *device)
{
    if(gizmo_rr)
        return(false);

    gizmo_rr=new RenderResource(device);

    if(!InitGizmoResource3D(device))
        return(false);

    if(!InitGizmoResource2D(device))
        return(false);

    return(true);
}

void FreeGizmoResource()
{
    SAFE_CLEAR_OBJECT_ARRAY(gizmo_prim)
    SAFE_CLEAR(gizmo_triangle.prim_creater);
    SAFE_CLEAR(gizmo_triangle.vdm);
//    SAFE_CLEAR(gizmo_pipeline_triangles);
//    SAFE_CLEAR_OBJECT_ARRAY(gizmo_mi_triangles)
    //SAFE_CLEAR(gizmo_mtl_triangles);
    SAFE_CLEAR(gizmo_rr);
}

VK_NAMESPACE_END