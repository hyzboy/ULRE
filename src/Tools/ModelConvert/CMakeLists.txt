# AssimpImport = 旧 Assimp 转换器。**已停用，源码存档保留。**
#
# 不参与构建，也无法按现状构建：
#   · 源码停在旧引擎头文件时代 —— AssimpLoader.h 引 hgl/math/Math.h，该头已删除；
#   · 引用的 assimp / IrrXML / zlibstatic 三个 target 在现工程里也不存在。
#
# 本目录不被 src/Tools/CMakeLists.txt add_subdirectory，整份 CMake 逻辑一并注释掉，
# 以免日后有人看到 add_executable 就以为"能编，只是没接进去"。
# 若将来要复活：先整体改写到现 hgl 头与现依赖，再把下面整段取消注释并接回构建。
#
# add_executable(AssimpImport EXCLUDE_FROM_ALL AssimpImport.cpp AssimpLoader.cpp AssimpLoader.h)
#
# target_link_libraries(AssimpImport  ${ULRE}
#                                     assimp
#                                     IrrXML
#                                     zlibstatic)
#
# set_property(TARGET AssimpImport PROPERTY FOLDER "ULRE/Tools")
