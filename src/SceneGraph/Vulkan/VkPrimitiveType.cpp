#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/type/StrChar.h>

namespace
{
    using namespace hgl::graph;

    struct PrimitiveNameInfo
    {
        const char *name;
        const int   len;
        PrimitiveType        prim;
    };

    #define PRIM_NAME(L,S) {S,sizeof(S)-1,PrimitiveType::L}

    constexpr const PrimitiveNameInfo PrimitiveInfoList[]=
    {
        PRIM_NAME(Points,           "points"),
        PRIM_NAME(Lines,            "lines"),
        PRIM_NAME(LineStrip,        "line_strip"),
        PRIM_NAME(Triangles,        "triangles"),
        PRIM_NAME(TriangleStrip,    "triangle_strip"),
        PRIM_NAME(Fan,              "fan"),
        PRIM_NAME(LinesAdj,         "lines_adjacency"),
        PRIM_NAME(LineStripAdj,     "line_strip_adjacency"),
        PRIM_NAME(TrianglesAdj,     "triangles_adjacency"),
        PRIM_NAME(TriangleStripAdj, "triangle_strip_adjacency"),
        PRIM_NAME(Patchs,           "patchs"),
    
        PRIM_NAME(SolidRectangles,  "solid_rectangles"),
        PRIM_NAME(WireRectangles,   "wire_rectangles"),
        //PRIM_NAME(SolidCube,        "solid_cube"),
        //PRIM_NAME(WireCube,         "wire_cube")

        PRIM_NAME(OBB,              "obb")
    };

    #undef PRIM_NAME
}//namespace

namespace hgl
{
    namespace graph
    {
const char *GetPrimName(const PrimitiveType &prim)
{
    for(const auto &pni:PrimitiveInfoList)
        if(pni.prim==prim)
            return pni.name;

    return nullptr;
}

const PrimitiveType ParsePrimitiveType(const char *name,int len)
{
    if(!name||!*name)return PrimitiveType::Error;

    if(len==0)
        len=hgl::strlen(name);

    for(const auto &pni:PrimitiveInfoList)
    {
        if(!hgl::stricmp(pni.name,name,len))
            return pni.prim;
    }

    return PrimitiveType::Error;
}

bool CheckGeometryShaderIn(const PrimitiveType &ip)
{
    if(ip==PrimitiveType::Points)return(true);
    if(ip==PrimitiveType::Lines)return(true);
    if(ip==PrimitiveType::LinesAdj)return(true);
    if(ip==PrimitiveType::Triangles)return(true);
    if(ip==PrimitiveType::TrianglesAdj)return(true);

    return(false);
}

bool CheckGeometryShaderOut(const PrimitiveType &op)
{
    if(op==PrimitiveType::Points)return(true);
    if(op==PrimitiveType::LineStrip)return(true);
    if(op==PrimitiveType::TriangleStrip)return(true);

    return(false);
}
    }//namespace graph
}//namespace hgl
