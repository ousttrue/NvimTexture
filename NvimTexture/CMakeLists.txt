set(TARGET_NAME NvimTexture)
add_library(${TARGET_NAME} nvim_pipe.cpp nvim_renderer.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE msgpackpp plog)
