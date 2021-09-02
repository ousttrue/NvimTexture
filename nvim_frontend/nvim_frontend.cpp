#include "nvim_frontend.h"
#include "nvim_grid.h"
#include "nvim_pipe.h"
#include "nvim_redraw.h"
#include <asio.hpp>
#include <msgpackpp/msgpackpp.h>
#include <msgpackpp/rpc.h>
#include <msgpackpp/windows_pipe_transport.h>
#include <plog/Log.h>
#include <thread>
#include <vector>

static std::vector<char> ParseConfig(const msgpackpp::parser &config_node) {
  auto p = config_node.get_string();
  std::string path(p.begin(), p.end());
  path += "\\init.vim";

  HANDLE config_file =
      CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  std::vector<char> guifont_out;
  if (config_file == INVALID_HANDLE_VALUE) {
    return guifont_out;
  }

  LARGE_INTEGER file_size;
  if (!GetFileSizeEx(config_file, &file_size)) {
    CloseHandle(config_file);
    return guifont_out;
  }
  std::vector<char> buffer(file_size.QuadPart);
  DWORD bytes_read;
  if (!ReadFile(config_file, buffer.data(), buffer.size(), &bytes_read, NULL)) {
    CloseHandle(config_file);
    return guifont_out;
  }
  CloseHandle(config_file);

  char *strtok_context;
  char *line = strtok_s(buffer.data(), "\r\n", &strtok_context);
  while (line) {
    char *guifont = strstr(line, "set guifont=");
    if (guifont) {
      // Check if we're inside a comment
      auto leading_count = guifont - line;
      bool inside_comment = false;
      for (int i = 0; i < leading_count; ++i) {
        if (line[i] == '"') {
          inside_comment = !inside_comment;
        }
      }
      if (!inside_comment) {
        guifont_out.clear();

        int line_offset = (guifont - line + strlen("set guifont="));
        int guifont_strlen = strlen(line) - line_offset;
        int escapes = 0;
        for (int i = 0; i < guifont_strlen; ++i) {
          if (line[line_offset + i] == '\\' && i < (guifont_strlen - 1) &&
              line[line_offset + i + 1] == ' ') {
            guifont_out.push_back(' ');
            ++i;
            continue;
          }
          guifont_out.push_back(line[i + line_offset]);
        }
        guifont_out.push_back('\0');
      }
    }
    line = strtok_s(NULL, "\r\n", &strtok_context);
  }

  return guifont_out;
}

struct ThreadWork {
  asio::io_context &_context;
  asio::io_context::work _work;
  std::thread _t;
  ThreadWork(asio::io_context &context)
      : _context(context), _work(context), _t([&context]() { context.run(); }) {
  }
  ~ThreadWork() {
    _context.stop();
    _t.join();
  }
  ThreadWork(const ThreadWork &) = delete;
  ThreadWork &operator=(const ThreadWork &) = delete;
};

class NvimFrontendImpl {
  NvimPipe _pipe;
  Nvim::Grid _grid;
  NvimRedraw _redraw;
  asio::io_context _context;
  msgpackpp::rpc_base<msgpackpp::WindowsPipeTransport> _rpc;

public:
  bool Launch(const wchar_t *command, const on_terminated_t &callback) {
    return _pipe.Launch(command, callback);
  }

  std::string Initialize() {

    _rpc.set_on_send([](auto data) {
      msgpackpp::parser msg(data);
      PLOGD << msg;
    });

    _rpc.attach(msgpackpp::WindowsPipeTransport(_context, _pipe.ReadHandle(),
                                                _pipe.WriteHandle()));
    ThreadWork sync(_context);

    {
      auto result = _rpc.request_async("nvim_get_api_info").get();
      // TODO:
      // mpack_node_t top_level_map =
      //     mpack_node_array_at(result.params, 1);
      // mpack_node_t version_map =
      //     mpack_node_map_value_at(top_level_map, 0);
      // int64_t api_level =
      //     mpack_node_map_cstr(version_map, "api_level")
      //         .data->value.i;
      // assert(api_level > 6);
    }

    { _rpc.notify("nvim_set_var", "nvy", 1); }

    std::string guifont;
    {
      auto result = _rpc.request_async("nvim_eval", "stdpath('config')").get();

      msgpackpp::parser msg(result);
      auto f = ParseConfig(msg);
      if (!f.empty()) {
        guifont = std::string(f.data());
      }
    }
    return guifont;
  }

  void AttachUI(NvimRenderer *renderer, int rows, int cols) {
    _rpc.add_proc("redraw",
                  [self = this, renderer](
                      const msgpackpp::parser &msg) -> std::vector<uint8_t> {
                    self->_redraw.Dispatch(&self->_grid, renderer, msg);
                    return {};
                  });

    ThreadWork sync(_context);

    {
      // Send UI attach notification
      msgpackpp::packer args;
      args.pack_array(3);
      args << cols;
      args << rows;
      args.pack_map(1);
      args << "ext_linegrid" << true;
      auto msg = msgpackpp::make_rpc_notify_packed("nvim_ui_attach",
                                                   args.get_payload());
      _rpc.write_async(msg);
    }
  }

  void Process() { _context.poll(); }

  void SendResize(int grid_rows, int grid_cols) {
    auto msg =
        msgpackpp::make_rpc_notify("nvim_ui_try_resize", grid_cols, grid_rows);
    _rpc.write_async(msg);
  }

  void SendMouseInput(Nvim::MouseButton button, Nvim::MouseAction action,
                      int mouse_row, int mouse_col) {
    bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x80) != 0;
    bool shift_down = (GetKeyState(VK_SHIFT) & 0x80) != 0;
    bool alt_down = (GetKeyState(VK_MENU) & 0x80) != 0;
    constexpr int MAX_INPUT_STRING_SIZE = 64;
    char input_string[MAX_INPUT_STRING_SIZE];
    snprintf(input_string, MAX_INPUT_STRING_SIZE, "%s%s%s",
             ctrl_down ? "C-" : "", shift_down ? "S-" : "",
             alt_down ? "M-" : "");

    auto msg = msgpackpp::make_rpc_notify(
        "nvim_input_mouse", GetMouseBotton(button), GetMouseAction(action),
        (const char *)input_string, 0, mouse_row, mouse_col);
    _rpc.write_async(msg);
  }

  void SendChar(wchar_t input_char) {
    // If the space is simply a regular space,
    // simply send the modified input
    if (input_char == VK_SPACE) {
      NvimSendModifiedInput("Space", true);
      return;
    }

    char utf8_encoded[64]{};
    if (!WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, 0, 0, NULL, NULL)) {
      return;
    }
    WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, utf8_encoded, 64, NULL,
                        NULL);

    auto msg =
        msgpackpp::make_rpc_notify("nvim_input", (const char *)utf8_encoded);
    _rpc.write_async(msg);
  }

  void SendSysChar(wchar_t input_char) {
    char utf8_encoded[64]{};
    if (!WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, 0, 0, NULL, NULL)) {
      return;
    }
    WideCharToMultiByte(CP_UTF8, 0, &input_char, 1, utf8_encoded, 64, NULL,
                        NULL);

    NvimSendModifiedInput(utf8_encoded, true);
  }

  void NvimSendModifiedInput(const char *input, bool virtual_key) {
    bool shift_down = (GetKeyState(VK_SHIFT) & 0x80) != 0;
    bool ctrl_down = (GetKeyState(VK_CONTROL) & 0x80) != 0;
    bool alt_down = (GetKeyState(VK_MENU) & 0x80) != 0;

    constexpr int MAX_INPUT_STRING_SIZE = 64;
    char input_string[MAX_INPUT_STRING_SIZE];

    snprintf(input_string, MAX_INPUT_STRING_SIZE, "<%s%s%s%s>",
             ctrl_down ? "C-" : "", shift_down ? "S-" : "",
             alt_down ? "M-" : "", input);

    auto msg =
        msgpackpp::make_rpc_notify("nvim_input", (const char *)input_string);
    _rpc.write_async(msg);
  }

  void SendInput(std::string_view input_chars) {
    auto msg = msgpackpp::make_rpc_notify("nvim_input", input_chars);
    _rpc.write_async(msg);
  }

  void OpenFile(const wchar_t *file_name) {
    char utf8_encoded[MAX_PATH]{};
    WideCharToMultiByte(CP_UTF8, 0, file_name, -1, utf8_encoded, MAX_PATH, NULL,
                        NULL);

    char file_command[MAX_PATH + 2] = {};
    strcpy_s(file_command, MAX_PATH, "e ");
    strcat_s(file_command, MAX_PATH - 3, utf8_encoded);

    _rpc.request_async("nvim_command", (const char *)file_command);
  }

  Nvim::GridSize GridSize() const { return _grid.Size(); }
  bool Sizing() const { return _redraw.Sizing(); }
  void SetSizing() { _redraw.SetSizing(); }
  const Nvim::HighlightAttribute *DefaultAttribute() const {
    return &_grid.hl(0);
  }
};

NvimFrontend::NvimFrontend() : _impl(new NvimFrontendImpl) {}
NvimFrontend::~NvimFrontend() { delete _impl; }
bool NvimFrontend::Launch(const wchar_t *command,
                          const on_terminated_t &callback) {
  return _impl->Launch(command, callback);
}
void NvimFrontend::AttachUI(NvimRenderer *renderer, int rows, int cols) {
  _impl->AttachUI(renderer, rows, cols);
}
void NvimFrontend::ResizeGrid(int rows, int cols) {
  _impl->SendResize(rows, cols);
}

std::tuple<std::string_view, float> NvimFrontend::Initialize() {
  auto guifont = _impl->Initialize();
  return NvimRedraw::ParseGUIFont(guifont);
}
void NvimFrontend::Process() { _impl->Process(); }
void NvimFrontend::Input(const Nvim::InputEvent &e) {
  switch (e.type) {
  case Nvim::InputEventTypes::Input:
    _impl->SendInput(e.input);
    break;
  case Nvim::InputEventTypes::ModifiedInput:
    _impl->NvimSendModifiedInput(e.input, true);
    break;
  case Nvim::InputEventTypes::Char:
    _impl->SendChar(e.ch);
    break;
  case Nvim::InputEventTypes::SysChar:
    _impl->SendSysChar(e.ch);
    break;
  default:
    assert(false);
    break;
  }
}
void NvimFrontend::Mouse(const Nvim::MouseEvent &e) {
  _impl->SendMouseInput(e.button, e.action, e.y, e.x);
}

void NvimFrontend::OpenFile(const wchar_t *file) { _impl->OpenFile(file); }

const Nvim::HighlightAttribute *NvimFrontend::DefaultAttribute() const {
  return _impl->DefaultAttribute();
}
Nvim::GridSize NvimFrontend::GridSize() const { return _impl->GridSize(); }
bool NvimFrontend::Sizing() const { return _impl->Sizing(); }
void NvimFrontend::SetSizing() { return _impl->SetSizing(); }
