#pragma once
#include <msgpackpp/msgpackpp.h>

class NvimRenderer {
public:
  void dispatch(msgpackpp::parser msg);

private:
  void option_set(const msgpackpp::parser &msg, int count);
};
