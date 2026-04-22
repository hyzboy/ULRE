# Tests for ULRE.ShaderGen
#
# MaterialKeyTest compiles MaterialKey.cpp directly so it does not pull in the
# full ULRE.ShaderGen static library (and its glslang/SPIRV-Cross transitive
# dependencies).  The global include_directories set by use_cm_module() in the
# root CMakeLists.txt provide all required header search paths.

add_executable(MaterialKeyTest
    MaterialKeyTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MaterialKey.cpp
)

target_link_libraries(MaterialKeyTest PRIVATE CMCore)

set_property(TARGET MaterialKeyTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME MaterialKeyTest COMMAND MaterialKeyTest)
