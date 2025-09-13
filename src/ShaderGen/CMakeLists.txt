set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/shadergen)

SET(SHADER_CREATER_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoMap.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoVertex.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoGeometry.h
                                ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoFragment.h)

SET(SHADER_CREATER_SOURCE_FILES ShaderCreateInfo.cpp
                                ShaderCreateInfoVertex.cpp
                                ShaderCreateInfoGeometry.cpp
                                ShaderCreateInfoFragment.cpp)

SOURCE_GROUP("Material Create Info\\Shader" FILES   ${SHADER_CREATER_HEADER_FILES}
                                                    ${SHADER_CREATER_SOURCE_FILES})

SET(MATERIAL_CREATE_INFO_HEADER_FILES ${SHADERGEN_INCLUDE_PATH}/MaterialCreateInfo.h)
SET(MATERIAL_CREATE_INFO_SOURCE_FILES MaterialCreateInfo.cpp)

SOURCE_GROUP("Material Create Info" FILES   ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                            ${MATERIAL_CREATE_INFO_SOURCE_FILES})

SET(DESC_INFO_HEADER_FILES  ${SHADERGEN_INCLUDE_PATH}/ShaderDescriptorInfo.h
                            ${SHADERGEN_INCLUDE_PATH}/MaterialDescriptorInfo.h)

SET(DESC_INFO_SOURCE_FILES  ShaderDescriptorInfo.cpp
                            MaterialDescriptorInfo.cpp)

SOURCE_GROUP("Material Create Info\\Descriptor Info" FILES  ${DESC_INFO_HEADER_FILES}
                                                            ${DESC_INFO_SOURCE_FILES})

SET(GLSL_COMPILER_SOURCE	GLSLCompiler.h
                            GLSLCompiler.cpp)

SOURCE_GROUP("GLSL Compiler" FILES ${GLSL_COMPILER_SOURCE})

set(STD_MTL_HEADER_PATH  ${ROOT_INCLUDE_PATH}/hgl/graph/mtl)

SET(SHADERGEN_COMMON_FILES  ${STD_MTL_HEADER_PATH}/UBOCommon.h
                            ${STD_MTL_HEADER_PATH}/SamplerName.h
                            ${STD_MTL_HEADER_PATH}/ShaderVariableType.h
                            ShaderVariableType.cpp
                            common/MFCommon.h
                            common/MFGetPosition.h
                            common/MFGetNormal.h
                            common/MFCamera.h
                            common/MFBillboard.h
)

SOURCE_GROUP("Common" FILES ${SHADERGEN_COMMON_FILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

SET(BLINN_PHONG_HEADER_FILES ${STD_MTL_HEADER_PATH}/BlinnPhong.h)

SET(STD_MTL_2D_SOURCE_FILES	${STD_MTL_HEADER_PATH}/Material2DCreateConfig.h
                            2d/Std2DMaterial.h
                            2d/Std2DMaterial.cpp
                            2d/Std2DMaterialLoader.cpp
                            2d/M_VertexColor2D.cpp
                            2d/M_PureColor2D.cpp
                            2d/M_PureTexture2D.cpp
                            2d/M_RectTexture2D.cpp
                            2d/M_RectTexture2DArray.cpp
                            2d/M_Text.cpp
)

SET(STD_MTL_3D_SOURCE_FILES	${STD_MTL_HEADER_PATH}/Material3DCreateConfig.h
                            3d/Std3DMaterial.h
                            3d/Std3DMaterial.cpp
                            3d/Std3DMaterialLoader.cpp
                            3d/M_PureColor3D.cpp
                            3d/M_VertexColor3D.cpp
                            3d/M_VertexLum3D.cpp
                            3d/M_BillboardDynamicSize.cpp
                            3d/M_BillboardFixedSize.cpp
                            3d/M_Billboard.cpp
                            3d/M_Gizmo3D.cpp
                            3d/M_SkyMinimal.cpp
                            3d/M_BasicLit.cpp
)

SET(STD_MTL_SOURCE  ${STD_MTL_HEADER_PATH}/MaterialCreateConfig.h
                    ${STD_MTL_HEADER_PATH}/StdMaterial.h
                    ${STD_MTL_HEADER_PATH}/ShaderBufferSource.h
                    MaterialCreateConfig.cpp
                    StandardMaterial.cpp
                    MaterialFileData.h
                    MaterialFileLoader.cpp
                    GLSLStructSize.cpp
)

SOURCE_GROUP("Standard Material" FILES ${STD_MTL_SOURCE})

SOURCE_GROUP("Standard Material\\Light Model\\Blinn Phong" FILES ${BLINN_PHONG_HEADER_FILES})

SOURCE_GROUP("Standard Material\\2D" FILES ${STD_MTL_2D_SOURCE_FILES})
SOURCE_GROUP("Standard Material\\3D" FILES ${STD_MTL_3D_SOURCE_FILES})

SET(SHADER_LIBRARY_FILES    ShaderLibrary.cpp
                            ShaderLibrary.h
                            MaterialLibrary.cpp
                            ${STD_MTL_HEADER_PATH}/MaterialLibrary.h)

SOURCE_GROUP("Shader Library" FILES ${SHADER_LIBRARY_FILES})

add_cm_library(ULRE.ShaderGen "ULRE"    ${DESC_INFO_HEADER_FILES}
                                        ${DESC_INFO_SOURCE_FILES}
                                        ${MATERIAL_CREATE_INFO_HEADER_FILES}
                                        ${MATERIAL_CREATE_INFO_SOURCE_FILES}
                                        ${SHADER_CREATER_HEADER_FILES}
                                        ${SHADER_CREATER_SOURCE_FILES}
                                        ${GLSL_COMPILER_SOURCE}
                                        ${SHADERGEN_COMMON_FILES}
                                        ${SHADER_LIBRARY_FILES}
                                        ${STD_MTL_SOURCE}
                                        ${BLINN_PHONG_HEADER_FILES}
                                        ${STD_MTL_2D_SOURCE_FILES}
                                        ${STD_MTL_3D_SOURCE_FILES}
)
