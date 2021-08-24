#include "nvim_dispatcher.h"
#include <plog/Log.h>
/**
 * https://neovim.io/doc/user/ui.html
 */

void NvimRenderer::dispatch(msgpackpp::parser msg) {

  auto count = msg.count();
  auto first = msg.first_array_item();
  auto name = first.value.get_string();
  auto remain = first.value.next();
  if (name == "option_set") {
    option_set(remain, count - 1);
  } else if (name == "resize") {

  } else if (name == "clear") {

  } else if (name == "eol_clear") {

  } else if (name == "mode_change") {

  } else if (name == "mode_info_set") {

  } else if (name == "mouse_off") {

  } else if (name == "win_viewport") {

  } else if (name == "default_colors_set") {

  } else if (name == "highlight_set") {

  } else if (name == "hl_group_set") {

  } else if (name == "update_fg") {

  } else if (name == "update_bg") {

  } else if (name == "update_sp") {

  } else if (name == "busy_start") {

  } else if (name == "busy_stop") {

  } else if (name == "cursor_goto") {

  } else if (name == "put") {

  } else if (name == "flush") {

  } else {
    PLOGD << name;
  }
}

void NvimRenderer::option_set(const msgpackpp::parser &msg, int count) {

  auto current = msg;
  for (int i = 0; i < count; ++i, current = current.next()) {
    PLOGD << current;
  }
}
