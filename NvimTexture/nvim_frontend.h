#pragma once
#include <memory>

class NvimFrontend {
  std::unique_ptr<class NvimFrontendImpl> _impl;

public:
  NvimFrontend();
  ~NvimFrontend();
  bool Launch();
  void Attach();
};
