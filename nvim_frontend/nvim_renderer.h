#pragma once
#include <string_view>

namespace Nvim {
struct HighlightAttribute;
class Grid;
} // namespace Nvim

class NvimRenderer {
public:
  virtual ~NvimRenderer(){};
  // font size
  virtual void SetFont(std::string_view font, float size) = 0;
  virtual std::tuple<float, float> FontSize() const = 0;
  // render
  virtual void DrawBackgroundRect(int rows, int cols,
                                  const Nvim::HighlightAttribute *hl) = 0;
  virtual void DrawGridLine(const Nvim::Grid *grid, int row) = 0;
  virtual void DrawCursor(const Nvim::Grid *grid) = 0;
  virtual void DrawBorderRectangles(const Nvim::Grid *grid, int width,
                                    int height) = 0;
  virtual std::tuple<int, int> StartDraw() = 0;
  virtual void FinishDraw() = 0;
};
