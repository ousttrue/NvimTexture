#pragma once
#include <functional>
#include <stdint.h>
#include <nvim_input.h>

class NvimWin32KeyProcessor {
  bool _dead_char_pending = false;

public:
  bool
  ProcessMessage(void *hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam,
                 const std::function<void(const Nvim::InputEvent &)> &input,
                 uint64_t *out);
};
