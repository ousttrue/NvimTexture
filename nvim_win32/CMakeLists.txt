set(TARGET_NAME nvim_win32)
add_library(${TARGET_NAME} nvim_win32_key_processor.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PUBLIC nvim_frontend)
