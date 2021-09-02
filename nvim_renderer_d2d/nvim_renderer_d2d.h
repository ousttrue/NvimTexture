#pragma once
#include <nvim_renderer.h>

struct HighlightAttribute;
class NvimGrid;
class NvimRendererD2D : public NvimRenderer {
  class NvimRendererD2DImpl *_impl = nullptr;

public:
  NvimRendererD2D(struct ID3D11Device *device,
                  const HighlightAttribute *defaultHL,
                  bool disable_ligatures = false, float linespace_factor = 1.0f,
                  uint32_t monitor_dpi = 96);
  ~NvimRendererD2D();
  void SetTarget(struct IDXGISurface2 *backbuffer);
  // font size
  void SetFont(std::string_view font, float size) override;
  std::tuple<float, float> FontSize() const override;
  // render
  std::tuple<int, int> StartDraw() override;
  void FinishDraw() override;
  void DrawBackgroundRect(int rows, int cols,
                          const HighlightAttribute *hl) override;
  void DrawGridLine(const NvimGrid *grid, int row) override;
  void DrawCursor(const NvimGrid *grid) override;
  void DrawBorderRectangles(const NvimGrid *grid, int width,
                            int height) override;
};
