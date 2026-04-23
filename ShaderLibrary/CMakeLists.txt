cmake_minimum_required(VERSION 3.28)

# Collect all GLSL shader files recursively
file(GLOB_RECURSE SHADER_LIBRARY_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.glsl"
)

# Create a source-only custom target so shader files appear in IDEs
# with proper folder hierarchy.
add_custom_target(ShaderLibrary
    SOURCES ${SHADER_LIBRARY_FILES}
)

# Reproduce the directory hierarchy as IDE source groups
foreach(_shader_file IN LISTS SHADER_LIBRARY_FILES)
    # Get the path relative to this CMakeLists.txt directory
    file(RELATIVE_PATH _rel_path "${CMAKE_CURRENT_SOURCE_DIR}" "${_shader_file}")
    # Strip the filename to get just the directory portion
    get_filename_component(_dir "${_rel_path}" DIRECTORY)
    # Replace forward/back slashes with backslashes for Visual Studio filters
    if(_dir)
        string(REPLACE "/" "\\" _group "ShaderLibrary\\${_dir}")
    else()
        set(_group "ShaderLibrary")
    endif()
    source_group("${_group}" FILES "${_shader_file}")
endforeach()

set_target_properties(ShaderLibrary PROPERTIES FOLDER "ULRE")
