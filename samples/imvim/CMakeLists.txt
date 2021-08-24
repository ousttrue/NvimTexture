set(TARGET_NAME imvim)
add_executable(${TARGET_NAME} main.cpp)
target_link_libraries(
  ${TARGET_NAME}
  PRIVATE imgui
          d3d11
          NvimTexture
          asio
          msgpackpp
          plog)
