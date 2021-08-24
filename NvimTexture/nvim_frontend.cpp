#include "nvim_frontend.h"
#include "nvim_dispatcher.h"
#include "nvim_pipe.h"
#include <asio.hpp>
#include <msgpackpp/msgpackpp.h>
#include <msgpackpp/rpc.h>
#include <msgpackpp/windows_pipe_transport.h>
#include <plog/Log.h>
#include <thread>

class NvimFrontendImpl {
  NvimPipe _pipe;
  asio::io_context _context;
  asio::io_context::work _work;
  msgpackpp::rpc_base<msgpackpp::WindowsPipeTransport> _rpc;
  std::thread _context_thead;
  NvimRenderer renderer;

  NvimFrontendImpl()
      : _work(_context),
        _context_thead([self = this]() { self->_context.run(); }) {
  }

  void Initialize() {
    _rpc.set_on_rpc_error(
        [](msgpackpp::rpc_errors ec, const msgpackpp::parser &msg) {
          //
          PLOGE << "[rpc_error]" << (int)ec;
        });
    _rpc.set_on_send([](const std::vector<uint8_t> &data) {
      msgpackpp::parser msg(data);
      PLOGD << "=> " << msg;
    });
    _rpc.set_on_msg([](const msgpackpp::parser &msg) {
      switch (msg[0].get_number<int>()) {
      case 0:
        PLOGD << "<= (request) " << msg;
        break;

      case 1:
        PLOGD << "<= (response) " << msg[1].get_number<int>();
        break;

      case 2:
        PLOGD << "<= (notify) " << msg[1].get_string();
        break;
      }
    });
    _rpc.attach(msgpackpp::WindowsPipeTransport(_context, _pipe.ReadHandle(),
                                                _pipe.WriteHandle()));

    {
      auto result = _rpc.request_async("nvim_get_api_info").get();
      // std::cout << msgpackpp::parser(result) << std::endl;
    }

    { _rpc.notify("nvim_set_var", "nvy", 1); }

    {
      auto result = _rpc.request_async("nvim_eval", "stdpath('config')").get();
      // std::cout << msgpackpp::parser(result) << std::endl;
    }
  }

public:
  ~NvimFrontendImpl() {
    _context.stop();
    _context_thead.join();
  }

  static std::unique_ptr<NvimFrontendImpl> Launch() {
    auto p = std::unique_ptr<NvimFrontendImpl>(new NvimFrontendImpl);
    if (!p->_pipe.Launch("nvim --embed")) {
      return nullptr;
    }

    p->Initialize();

    return p;
  }

  void Attach() {

    _rpc.add_proc(
        "redraw",
        [self = this](const msgpackpp::parser &commands) -> msgpackpp::bytes {
          //
          assert(commands.is_array());
          auto count = commands.count().value;
          auto command = commands.first_array_item().value;
          for (uint32_t i = 0; i < count; ++i, command = command.next()) {
            self->renderer.dispatch(command);
          }

          return {};
        });

    // start rendering
    {
      msgpackpp::packer args;
      args.pack_array(3);
      args << 190;
      args << 45;
      args.pack_map(1);
      args << "ext_linegrid" << true;
      _rpc.notify_raw("nvim_ui_attach", args.get_payload());
    }

    { _rpc.notify("nvim_ui_try_resize", 190, 45); }
  }
};

NvimFrontend::NvimFrontend() {}

NvimFrontend::~NvimFrontend() {}

bool NvimFrontend::Launch() {
  _impl = NvimFrontendImpl::Launch();
  return _impl != nullptr;
}

void NvimFrontend::Attach() { _impl->Attach(); }
