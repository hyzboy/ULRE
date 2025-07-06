#pragma once

#include<hgl/graph/mtl/StdMaterial.h>
#include<hgl/graph/CoordinateSystem.h>
STD_MTL_NAMESPACE_BEGIN
namespace func
{
    constexpr const char *GetPosition2D[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        "vec4 GetPosition2D(){return vec4(Position,0,1);}",                                         //NDC
        "vec4 GetPosition2D(){return vec4(Position.xy*2-1,0,1);}",                                  //ZeroToOne
        "vec4 GetPosition2D(){return viewport.ortho_matrix*vec4(Position,0,1);}"                    //Ortho
    };

    constexpr const char *GetPosition2DL2W[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        "vec4 GetPosition2D(){return GetLocalToWorld()*vec4(Position,0,1);}",                       //NDC
        "vec4 GetPosition2D(){return GetLocalToWorld()*vec4(Position.xy*2-1,0,1);}",                //ZeroToOne
        "vec4 GetPosition2D(){return GetLocalToWorld()*viewport.ortho_matrix*vec4(Position,0,1);}"  //Ortho
    };

    constexpr const char *GetWorldPosition3D_VS     ="vec4 GetWorldPosition3D(){return vec4(Position,1);}";
    constexpr const char *GetWorldPosition3DL2W_VS  ="vec4 GetWorldPosition3D(){return GetLocalToWorld()*vec4(Position,1);}";
    constexpr const char *GetWorldPosition3D_Other  ="vec4 GetWorldPosition3D(){return WorldPosition;}";

    constexpr const char *GetPosition3D             ="vec4 GetPosition3D(){return vec4(Position,1);}";
    constexpr const char *GetPosition3DL2W          ="vec4 GetPosition3D(){return GetLocalToWorld()*vec4(Position,1);}";
    constexpr const char *GetPosition3DCamera       ="vec4 GetPosition3D(){return camera.vp*vec4(Position,1);}";
    constexpr const char *GetPosition3DL2WCamera    ="vec4 GetPosition3D(){return camera.vp*GetLocalToWorld()*vec4(Position,1);}";

    constexpr const char *GetPosition3DBy2D         ="vec4 GetPosition3D(){return vec4(Position,0,1);}";
    constexpr const char *GetPosition3DL2WBy2D      ="vec4 GetPosition3D(){return GetLocalToWorld()*vec4(Position,0,1);}";
    constexpr const char *GetPosition3DCameraBy2D   ="vec4 GetPosition3D(){return camera.vp*vec4(Position,0,1);}";
    constexpr const char *GetPosition3DL2WCameraBy2D="vec4 GetPosition3D(){return camera.vp*GetLocalToWorld()*vec4(Position,0,1);}";
}//namespace func
STD_MTL_NAMESPACE_END
