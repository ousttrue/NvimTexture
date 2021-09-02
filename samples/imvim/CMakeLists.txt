set(TARGET_NAME imvim)
add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(${TARGET_NAME} PRIVATE imgui d3d11 nvim_frontend
                                             nvim_renderer_d2d plog)
set_property(TARGET ${TARGET_NAME} PROPERTY CXX_STANDARD 20)
