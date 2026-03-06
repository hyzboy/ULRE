#pragma once

#include<hgl/mtl/StdMaterial.h>
#include<hgl/graph/data/CoordinateSystem.h>
namespace hgl::graph::mtl{
namespace func
{
    // ═════════════════════════════════════════════════════════════════════════
    // Phase B 规范化 Position Helper（强制使用，符合 SHADER_HELPER_FUNCTION_SPEC.md）
    // ═════════════════════════════════════════════════════════════════════════
    // 规范约束：
    //   1) 新逻辑优先使用 GetLocalPosition/GetWorldPosition/GetClipPosition/GetScreenPosition
    //   2) 业务代码不再新增 GetPosition3D/GetWorldPosition3D 调用
    //   3) Legacy 名称仅用于历史材质兼容，Phase C 后统一移除
    
    /// Vertex Shader: 获取 Local Space Position（不经过任何变换）
    constexpr const char *GetLocalPosition_VS = 
        "vec3 GetLocalPosition(){return Position;}";
    
    /// Vertex Shader: 获取 World Space Position（应用 L2W 变换）
    constexpr const char *GetWorldPosition_VS = 
        "vec4 GetWorldPosition(){return GetLocalToWorld()*vec4(Position,1.0);}";
    
    /// Vertex Shader: 获取 Clip Space Position（应用 L2W + Camera VP 变换）
    constexpr const char *GetClipPosition_VS = 
        "vec4 GetClipPosition(){return camera.vp*GetLocalToWorld()*vec4(Position,1.0);}";
    
    /// Fragment/Geometry Shader: 获取 World Space Position（从插值输入读取）
    constexpr const char *GetWorldPosition_Other = 
        "vec4 GetWorldPosition(){return Input.WorldPosition;}";
    
    /// Fragment Shader: 获取 Screen Space Position
    constexpr const char *GetScreenPosition_FS = 
        "vec2 GetScreenPosition(){return gl_FragCoord.xy;}";

    /// 规范映射（文档/代码统一口径）
    ///   Canonical: GetLocalPosition / GetWorldPosition / GetClipPosition / GetScreenPosition
    ///   Legacy   : GetPosition3D / GetWorldPosition3D / GetPosition2D
    
    // ═════════════════════════════════════════════════════════════════════════
    // Legacy Helper（Phase C 前保留，Phase C 后移除）
    // ═════════════════════════════════════════════════════════════════════════
    
    constexpr const char *GetPosition2D[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        "vec4 GetPosition2D(){return vec4(Position.xy,0,1);}",                                      //NDC
        "vec4 GetPosition2D(){return vec4(Position.xy*2-1,0,1);}",                                  //ZeroToOne
        "vec4 GetPosition2D(){return viewport.ortho_matrix*vec4(Position.xy,0,1);}"                 //Ortho
    };

    constexpr const char *GetPosition2DL2W[size_t(CoordinateSystem2D::RANGE_SIZE)]=
    {
        "vec4 GetPosition2D(){return GetLocalToWorld()*vec4(Position.xy,0,1);}",                    //NDC
        "vec4 GetPosition2D(){return GetLocalToWorld()*vec4(Position.xy*2-1,0,1);}",                //ZeroToOne
        "vec4 GetPosition2D(){return GetLocalToWorld()*viewport.ortho_matrix*vec4(Position.xy,0,1);}"//Ortho
    };

    // DEPRECATED: 使用 GetWorldPosition_VS / GetWorldPosition_Other 替代
    constexpr const char *GetWorldPosition3D_VS     ="vec4 GetWorldPosition3D(){return vec4(Position,1);}";
    constexpr const char *GetWorldPosition3DL2W_VS  ="vec4 GetWorldPosition3D(){return GetLocalToWorld()*vec4(Position,1);}";
    constexpr const char *GetWorldPosition3D_Other  ="vec4 GetWorldPosition3D(){return WorldPosition;}";

    // DEPRECATED: 使用 GetClipPosition_VS 替代
    constexpr const char *GetPosition3D             ="vec4 GetPosition3D(){return vec4(Position,1);}";
    constexpr const char *GetPosition3DL2W          ="vec4 GetPosition3D(){return GetLocalToWorld()*vec4(Position,1);}";
    constexpr const char *GetPosition3DCamera       ="vec4 GetPosition3D(){return camera.vp*vec4(Position,1);}";
    constexpr const char *GetPosition3DL2WCamera    ="vec4 GetPosition3D(){return camera.vp*GetLocalToWorld()*vec4(Position,1);}";

    constexpr const char *GetPosition3DBy2D         ="vec4 GetPosition3D(){return vec4(Position,0,1);}";
    constexpr const char *GetPosition3DL2WBy2D      ="vec4 GetPosition3D(){return GetLocalToWorld()*vec4(Position,0,1);}";
    constexpr const char *GetPosition3DCameraBy2D   ="vec4 GetPosition3D(){return camera.vp*vec4(Position,0,1);}";
    constexpr const char *GetPosition3DL2WCameraBy2D="vec4 GetPosition3D(){return camera.vp*GetLocalToWorld()*vec4(Position,0,1);}";
}//namespace func
}//namespace hgl::graph::mtl
