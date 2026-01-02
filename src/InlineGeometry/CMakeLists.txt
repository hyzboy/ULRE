set(SG_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/graph)

SET(GEOMETRY_UTILS_FILES    ${SG_INCLUDE_PATH}/geo/GeometryBuilder.h
                            ${SG_INCLUDE_PATH}/geo/IndexGenerator.h
                            GeometryBuilder.cpp
                            GeometryValidator.h)

SOURCE_GROUP("Utils" FILES ${GEOMETRY_UTILS_FILES})

SET(GEOMETRY_PRIMITIVES_BASIC   InlineGeometryCommon.h
                                ${SG_INCLUDE_PATH}/geo/InlineGeometry.h
                                PlaneAndSquare.cpp
                                Rectangle.cpp
                                Cube.cpp
                                Circle.cpp
                                AxisBoundingBoxSquareArray.cpp)

SET(GEOMETRY_PRIMITIVES_SPHERE  SphereDomeTorus.cpp
                                HexSphere.cpp)

SET(GEOMETRY_PRIMITIVES_CYLINDER CylinderConeTorusIndices.cpp
                                 Cylinder.cpp
                                 HollowCylinder.cpp
                                 Cone.cpp)

SET(GEOMETRY_PRIMITIVES_CAPSULE Capsule.cpp
                                TaperedCapsule.cpp)

SET(GEOMETRY_PRIMITIVES_EXTENDED Frustum.cpp
                                 Arrow.cpp
                                 RoundedBox.cpp
                                 PipeElbow.cpp
                                 Helix.cpp
                                 SolidGear.cpp
                                 HollowGear.cpp
                                 Heart.cpp
                                 Star.cpp
                                 Polygon.cpp
                                 Wedge.cpp
                                 Teardrop.cpp
                                 Egg.cpp
                                 Crescent.cpp
                                 StraightStairs.cpp
                                 SpiralStairs.cpp
                                 Arch.cpp
                                 Tube.cpp
                                 Bolt.cpp
                                 Nut.cpp
                                 Diamond.cpp
                                 Crystal.cpp
                                 Cross.cpp
                                 Ribbon.cpp
                                 PhoneShapedBox.cpp
                                 TorusKnot.cpp
                                 MobiusStrip.cpp
                                 KleinBottle.cpp
                                 Superellipsoid.cpp)

SET(GEOMETRY_PRIMITIVES_ADVANCED ${SG_INCLUDE_PATH}/geo/Extruded.h
                                 ${SG_INCLUDE_PATH}/geo/Wall.h
                                 ExtrudedPolygon.cpp
                                 Wall.cpp)

SOURCE_GROUP("Primitives\\Basic" FILES ${GEOMETRY_PRIMITIVES_BASIC})
SOURCE_GROUP("Primitives\\Sphere" FILES ${GEOMETRY_PRIMITIVES_SPHERE})
SOURCE_GROUP("Primitives\\Cylinder" FILES ${GEOMETRY_PRIMITIVES_CYLINDER})
SOURCE_GROUP("Primitives\\Capsule" FILES ${GEOMETRY_PRIMITIVES_CAPSULE})
SOURCE_GROUP("Primitives\\Extended" FILES ${GEOMETRY_PRIMITIVES_EXTENDED})
SOURCE_GROUP("Primitives\\Advanced" FILES ${GEOMETRY_PRIMITIVES_ADVANCED})

SET(GEOMETRY_FILES  ${GEOMETRY_UTILS_FILES}
                    ${GEOMETRY_PRIMITIVES_BASIC}
                    ${GEOMETRY_PRIMITIVES_SPHERE}
                    ${GEOMETRY_PRIMITIVES_CYLINDER}
                    ${GEOMETRY_PRIMITIVES_CAPSULE}
                    ${GEOMETRY_PRIMITIVES_EXTENDED}
                    ${GEOMETRY_PRIMITIVES_ADVANCED})

add_cm_library(ULRE.InlineGeometry "ULRE"   ${GEOMETRY_FILES}
)
