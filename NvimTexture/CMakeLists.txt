set(TARGET_NAME NvimTexture)
add_library(${TARGET_NAME} nvim_pipe.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
