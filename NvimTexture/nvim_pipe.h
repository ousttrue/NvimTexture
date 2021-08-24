#pragma once
#include <memory>

class NvimPipe {
  std::unique_ptr<class NvimPipeImpl> _impl;

public:
  NvimPipe();
  ~NvimPipe();
  bool Launch(const char *command);
  void* ReadHandle();
  void* WriteHandle();
};
