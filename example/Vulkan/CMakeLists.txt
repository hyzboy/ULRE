macro(CreateProject group name)
    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Vulkan/${group}")
endmacro()

CreateProject("Basic" 01_draw_triangle_in_NDC           first_triangle.cpp)
CreateProject("Basic" 02_draw_triangle_use_UBO          second_triangle.cpp)
CreateProject("Basic" 03_auto_instance                  third_triangle.cpp)
CreateProject("Basic" 04_auto_merge_material_instance   fourth_triangle.cpp)

#CreateProject("Basic" FragCoord             FragCoordTest.cpp)

CreateProject("Texture" 05_texture_format   TextureFormat.cpp)
CreateProject("Texture" 06_texture_quad     texture_quad.cpp)
CreateProject("Texture" 07_texture_rect     texture_rect.cpp)
#CreateProject("Texture" HQFilterTexture HQFilterTexture.cpp)
#CreateProject(06.Geometry2D Geometry2D.cpp)

#CreateProject("Tile" DrawTile              DrawTile.cpp)
#CreateProject("Tile" DrawText              DrawText.cpp)

#CreateProject("Geometry3D " Geometry3D            Geometry3D.cpp)

#CreateProject("Scene" SceneTree             SceneTree.cpp)
#CreateProject("Scene" LoadStaticMesh        LoadStaticMesh.cpp LoadScene.cpp)
#CreateProject("Scene" InlineGeometryScene   InlineGeometryScene.cpp)

#CreateProject("Scene/Sky" SkyColor              SkyColor.cpp)
#CreateProject("Scene/Sky" Atmosphere            Atmosphere.cpp)

#CreateProject("Advanced Rendering" OffscreenRender       OffscreenRender.cpp)

#CreateProject(12.PBRBasic PBRBasic.cpp)
#CreateProject(12.Deferred Deferred.cpp)
#CreateProject("Advanced Rendering" DeferredRender        DeferredRender.cpp)
#CreateProject(14.AutoMaterial auto_material.cpp)

#CreateProject("Texture" Cubemap               Cubemap.cpp)
#CreateProject("Texture" EquirectangularMap    EquirectangularMap.cpp)
#CreateProject("Picking" RayPicking            RayPicking.cpp)

#CreateProject("Landscape" TerrainSimple         TerrainSimple.cpp)
