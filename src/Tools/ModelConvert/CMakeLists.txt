add_executable(ModelConvert MainUnit.cpp AssimpLoader.cpp AssimpLoader.h FBXLoader.cpp FBXLoader.h)

target_link_libraries(ModelConvert  ${ULRE}
									assimp
                                    IrrXML
                                    zlibstatic)

# FBX SDK integration (when available)
# find_package(FBX QUIET)
# if(FBX_FOUND)
#     target_compile_definitions(ModelConvert PRIVATE HAVE_FBX_SDK)
#     target_include_directories(ModelConvert PRIVATE ${FBX_INCLUDE_DIR})
#     target_link_libraries(ModelConvert ${FBX_LIBRARIES})
# endif()

set_property(TARGET ModelConvert PROPERTY FOLDER "ULRE/Tools")