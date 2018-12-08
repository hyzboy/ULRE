add_executable(TextureSampler main.cpp TGATexture.cpp)

target_link_libraries(TextureSampler PRIVATE ${ULRE})

