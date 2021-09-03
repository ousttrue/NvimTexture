#include "nvim_win32_key_processor.h"
#include <Windows.h>

static const char *VKMap(int virtual_key, bool has_control) {
  switch (virtual_key) {
  case VK_BACK:
    return "BS";
  case VK_TAB:
    return "Tab";
  case VK_RETURN:
    return "CR";
  case VK_ESCAPE:
    return "Esc";
  case VK_PRIOR:
    return "PageUp";
  case VK_NEXT:
    return "PageDown";
  case VK_HOME:
    return "Home";
  case VK_END:
    return "End";
  case VK_LEFT:
    return "Left";
  case VK_UP:
    return "Up";
  case VK_RIGHT:
    return "Right";
  case VK_DOWN:
    return "Down";
  case VK_INSERT:
    return "Insert";
  case VK_DELETE:
    return "Del";
  case VK_NUMPAD0:
    return "k0";
  case VK_NUMPAD1:
    return "k1";
  case VK_NUMPAD2:
    return "k2";
  case VK_NUMPAD3:
    return "k3";
  case VK_NUMPAD4:
    return "k4";
  case VK_NUMPAD5:
    return "k5";
  case VK_NUMPAD6:
    return "k6";
  case VK_NUMPAD7:
    return "k7";
  case VK_NUMPAD8:
    return "k8";
  case VK_NUMPAD9:
    return "k9";
  case VK_MULTIPLY:
    return "kMultiply";
  case VK_ADD:
    return "kPlus";
  case VK_SEPARATOR:
    return "kComma";
  case VK_SUBTRACT:
    return "kMinus";
  case VK_DECIMAL:
    return "kPoint";
  case VK_DIVIDE:
    return "kDivide";
  case VK_F1:
    return "F1";
  case VK_F2:
    return "F2";
  case VK_F3:
    return "F3";
  case VK_F4:
    return "F4";
  case VK_F5:
    return "F5";
  case VK_F6:
    return "F6";
  case VK_F7:
    return "F7";
  case VK_F8:
    return "F8";
  case VK_F9:
    return "F9";
  case VK_F10:
    return "F10";
  case VK_F11:
    return "F11";
  case VK_F12:
    return "F12";
  case VK_F13:
    return "F13";
  case VK_F14:
    return "F14";
  case VK_F15:
    return "F15";
  case VK_F16:
    return "F16";
  case VK_F17:
    return "F17";
  case VK_F18:
    return "F18";
  case VK_F19:
    return "F19";
  case VK_F20:
    return "F20";
  case VK_F21:
    return "F21";
  case VK_F22:
    return "F22";
  case VK_F23:
    return "F23";
  case VK_F24:
    return "F24";
  case VK_OEM_2:
    if (has_control) {
      // C-/
      return "/";
    }
  }

  return nullptr;
}

bool NvimWin32KeyProcessor::ProcessMessage(
    void *hwnd, uint32_t msg, uint64_t wparam, uint64_t lparam,
    const std::function<void(const Nvim::InputEvent &)> &_on_input,
    uint64_t *out) {

  switch (msg) {
  case WM_DEADCHAR:
  case WM_SYSDEADCHAR: {
    _dead_char_pending = true;
    *out = 0;
    return true;
  }

  case WM_CHAR: {
    _dead_char_pending = false;
    // Special case for <LT>
    if (wparam == 0x3C) {
      _on_input(Nvim::InputEvent::create_input("<LT>"));
    } else {
      _on_input(Nvim::InputEvent::create_char(wparam));
    }
    return 0;
  }

  case WM_SYSCHAR: {
    _dead_char_pending = false;
    _on_input(Nvim::InputEvent::create_syschar(wparam));
    return 0;
  }

  case WM_KEYDOWN:
  case WM_SYSKEYDOWN: {
    if (((GetKeyState(VK_MENU) & 0x80) != 0) && wparam == VK_RETURN) {
      // Special case for <ALT+ENTER> (fullscreen transition)
      // ToggleFullscreen();
    } else {
      LONG msg_pos = GetMessagePos();
      POINTS pt = MAKEPOINTS(msg_pos);
      MSG current_msg{.hwnd = (HWND)hwnd,
                      .message = msg,
                      .wParam = wparam,
                      .lParam = (LPARAM)lparam,
                      .time = static_cast<DWORD>(GetMessageTime()),
                      .pt = POINT{pt.x, pt.y}};

      if (_dead_char_pending) {
        if (static_cast<int>(wparam) == VK_SPACE ||
            static_cast<int>(wparam) == VK_BACK ||
            static_cast<int>(wparam) == VK_ESCAPE) {
          _dead_char_pending = false;
          TranslateMessage(&current_msg);
          return 0;
        }
      }

      // If none of the special keys were hit, process in
      // WM_CHAR
      auto key = VKMap(static_cast<int>(wparam), GetKeyState(VK_CONTROL) < 0);
      if (key) {
        _on_input(Nvim::InputEvent::create_modified(key));
      } else {
        TranslateMessage(&current_msg);
      }
    }
    return 0;
  }
  }

  return false;
}
