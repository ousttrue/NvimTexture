#pragma once
#include <string_view>
#include <tuple>

namespace msgpackpp {
class parser;
}

namespace Nvim {
class Grid;
}

struct NvimRedraw {
  bool _ui_busy = false;

  void Dispatch(Nvim::Grid *grid, class NvimRenderer *renderer,
                const msgpackpp::parser &params);
  static std::tuple<std::string_view, float>
  ParseGUIFont(std::string_view gui_font);

  bool _sizing = false;
  bool Sizing() const { return _sizing; }
  void SetSizing() { _sizing = true; }

private:
  void SetGuiOptions(class NvimRenderer *renderer,
                     const msgpackpp::parser &option_set);
  void UpdateGridSize(Nvim::Grid *grid, const msgpackpp::parser &grid_resize);
  void UpdateCursorPos(Nvim::Grid *grid, const msgpackpp::parser &cursor_goto);
  void UpdateCursorModeInfos(Nvim::Grid *grid,
                             const msgpackpp::parser &mode_info_set_params);
  void UpdateCursorMode(Nvim::Grid *grid, const msgpackpp::parser &mode_change);
  void UpdateDefaultColors(Nvim::Grid *grid,
                           const msgpackpp::parser &default_colors);
  void UpdateHighlightAttributes(Nvim::Grid *grid,
                                 const msgpackpp::parser &highlight_attribs);
  void DrawGridLines(Nvim::Grid *grid, NvimRenderer *renderer,
                     const msgpackpp::parser &grid_lines);
  void ScrollRegion(Nvim::Grid *grid, NvimRenderer *renderer,
                    const msgpackpp::parser &scroll_region);
};
