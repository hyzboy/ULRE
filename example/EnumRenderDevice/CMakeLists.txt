add_executable(EnumRenderDevice main.cpp)

target_link_libraries(EnumRenderDevice PRIVATE MathGeoLib glfw GL ULRE.RenderDevice)
