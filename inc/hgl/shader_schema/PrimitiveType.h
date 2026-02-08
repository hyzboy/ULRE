#pragma once

#include<hgl/CoreType.h>
#include<hgl/type/EnumUtil.h>

namespace hgl::graph
{
	/**
	 * Primitive type enumeration
	 */
	enum class PrimitiveType:uint32
	{
		Points=0,                           ///< Points
		Lines,                              ///< Lines
		LineStrip,                          ///< Line strip
		Triangles,                          ///< Triangles
		TriangleStrip,                      ///< Triangle strip
		Fan,                                ///< Triangle fan
		LinesAdj,                           ///< Adjacency lines
		LineStripAdj,                       ///< Adjacency line strip
		TrianglesAdj,                       ///< Adjacency triangles
		TriangleStripAdj,                   ///< Adjacency triangle strip
		Patchs,

		// 2D elements
		SolidRectangles=0x100,              ///< Solid rectangles
		SolidCircles,                       ///< Solid circles

		WireRectangles=0x200,               ///< Wire rectangles
		WireCircles,                        ///< Wire circles

		// Special elements
		Billboard=0x500,                    ///< Billboard

		OBB=0x600,                          ///< OBB

		ENUM_CLASS_RANGE(Points,Patchs),

		Error
	};//

	const char *GetPrimName(const PrimitiveType &prim);
	const PrimitiveType ParsePrimitiveType(const char *name,int len=0);

	bool CheckGeometryShaderIn(const PrimitiveType &);
	bool CheckGeometryShaderOut(const PrimitiveType &);
}//namespace hgl::graph
