# Uses CreateProject macro and include_directories from parent (example/Geometry/CMakeLists.txt)

CreateProject(LoadScene   ../Shared/GeometryLoader.cpp ../Shared/LoadStaticMesh.cpp LoadScene.cpp)
