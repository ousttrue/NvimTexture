// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ folder + read
// the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <nvim_frontend.h>
#include <nvim_grid.h>
#include <nvim_renderer_d2d.h>
#include <nvim_win32_key_processor.h>
#include <optional>
#include <plog/Appenders/DebugOutputAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>
#include <tchar.h>
#include <wrl/client.h>

template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd,
                                                             UINT msg,
                                                             WPARAM wparam,
                                                             LPARAM lparam);

NvimWin32KeyProcessor g_nvimKey;

// Win32 message handler
LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_CREATE) {
    auto createStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
    SetWindowLongPtr(hwnd, GWLP_USERDATA,
                     reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
    return 0;
  }

  auto p = GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (p) {
    auto nvim = reinterpret_cast<NvimFrontend *>(p);
    uint64_t out;
    if (g_nvimKey.ProcessMessage(
            hwnd, msg, wparam, lparam,
            [nvim](const Nvim::InputEvent &input) { nvim->Input(input); },
            &out)) {
      // TODO: require nvim focus control
      // return out;
    }
  }

  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
    return true;

  switch (msg) {
  case WM_SIZE:
    return 0;
  case WM_SYSCOMMAND:
    if ((wparam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProc(hwnd, msg, wparam, lparam);
}

class Win32Window {
  WNDCLASSEX _wc = {0};
  HWND _hwnd = nullptr;

public:
  ~Win32Window() {
    ::DestroyWindow(_hwnd);
    ::UnregisterClass(_wc.lpszClassName, _wc.hInstance);
  }
  HWND Create(void *p) {
    // Create application window
    // ImGui_ImplWin32_EnableDpiAwareness();
    _wc = {sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
           GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
           _T("ImGui Example"),   NULL};
    ::RegisterClassEx(&_wc);
    _hwnd = ::CreateWindow(
        _wc.lpszClassName, _T("Dear ImGui DirectX11 Example"),
        WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, _wc.hInstance, p);

    return _hwnd;
  }
};

class D3DManager {
public:
  ComPtr<ID3D11Device> _pd3dDevice;
  ComPtr<ID3D11DeviceContext> _pd3dDeviceContext;
  ComPtr<IDXGISwapChain> _pSwapChain;
  ComPtr<ID3D11RenderTargetView> _mainRenderTargetView;
  DXGI_SWAP_CHAIN_DESC _desc;

  ~D3DManager() {}

  bool Create(HWND hwnd) {
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // require D2D
#ifndef NDEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };
    if (D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &_pSwapChain,
            &_pd3dDevice, &featureLevel, &_pd3dDeviceContext) != S_OK) {
      return false;
    }
    _pSwapChain->GetDesc(&_desc);

    return true;
  }

  void PrepareBackbuffer(UINT w, UINT h, const float *clear_color_with_alpha) {
    if (_desc.BufferDesc.Width != w || _desc.BufferDesc.Height != h) {
      // resize
      _mainRenderTargetView.Reset();
      _pSwapChain->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0);
      _pSwapChain->GetDesc(&_desc);
    }

    if (!_mainRenderTargetView) {
      ComPtr<ID3D11Texture2D> pBackBuffer;
      _pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
      _pd3dDevice->CreateRenderTargetView(pBackBuffer.Get(), NULL,
                                          &_mainRenderTargetView);
    }

    _pd3dDeviceContext->OMSetRenderTargets(
        1, _mainRenderTargetView.GetAddressOf(), NULL);
    _pd3dDeviceContext->ClearRenderTargetView(_mainRenderTargetView.Get(),
                                              clear_color_with_alpha);
  }

  void Present() {
    _pSwapChain->Present(1, 0); // Present with vsync
    // g_pSwapChain->Present(0, 0); // Present without vsync
    ID3D11RenderTargetView *tmp[1] = {nullptr};
    _pd3dDeviceContext->OMSetRenderTargets(1, tmp, NULL);
  }
};

using RenderTargetRenderer_t =
    std::function<ID3D11ShaderResourceView *(int w, int h)>;

using Input_t = std::function<void(const Nvim::InputEvent &e)>;

struct DockNode {
  std::string name;
  ImGuiDir dir = {};
  float fraction = 0.5f;
  std::vector<DockNode> children;

  ImGuiID id = {};

  void split();
};

void DockNode::split() {
  ImGui::DockBuilderDockWindow(name.c_str(), id);
  if (!children.empty()) {
    auto &first = children.front();
    auto &second = children.back();
    ImGui::DockBuilderSplitNode(id, dir, fraction, &first.id, &second.id);
    first.split();
    second.split();
  }
}

auto LEFT = "LEFT";
auto CENTER = "CENTER";
auto RIGHT = "RIGHT";

class DockingSpace {
  ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
  bool _first_time = true;

  DockNode _layout_root = {
      "Root",
      ImGuiDir_Left,
      0.3f,
      {
          {LEFT},
          {"Right", ImGuiDir_Right, 0.4f, {{RIGHT}, {CENTER}}},
      }};

public:
  DockingSpace() {}

  void Initialize() {
    // enable
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  }

  void Draw() {

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    // menubar
    if (ImGui::BeginMenuBar()) {
      ImGui::EndMenuBar();
    }

    if (_first_time) {
      // layout dock nodes
      _first_time = false;

      ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
      _layout_root.id = ImGui::DockBuilderAddNode(
          dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);

      ImGui::DockBuilderSetNodeSize(_layout_root.id, viewport->Size);

      _layout_root.split();
    }

    ImGui::End();
  }
};

class Gui {
  // Our state
  ImVec4 _clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  DockingSpace _dock_space;

public:
  float clear_color_with_alpha[4] = {0};

  Gui(HWND hwnd, ID3D11Device *device, ID3D11DeviceContext *context) {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable
    // Keyboard Controls io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; //
    // Enable Gamepad Controls

    _dock_space.Initialize();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(device, context);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can
    // also load multiple fonts and use ImGui::PushFont()/PopFont() to select
    // them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you
    // need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please
    // handle those errors in your application (e.g. use an assertion, or
    // display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and
    // stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame
    // below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string
    // literal you need to write a double backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    // ImFont* font =
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f,
    // NULL, io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);
  }
  ~Gui() {
    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
  }

  void Render(const RenderTargetRenderer_t &render, const Input_t &input) {
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    _dock_space.Draw();

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair
    // to created a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin(LEFT); // Create a window called "Hello, world!"
                          // and append into it.

      ImGui::Text("This is some useful text."); // Display some text (you can
                                                // use a format strings too)
      ImGui::SliderFloat("float", &f, 0.0f,
                         1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
      ImGui::ColorEdit3(
          "clear color",
          (float *)&_clear_color); // Edit 3 floats representing a color

      if (ImGui::Button("Button")) // Buttons return true when clicked (most
                                   // widgets return true when edited/activated)
        counter++;
      ImGui::SameLine();
      ImGui::Text("counter = %d", counter);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      ImGui::End();
    }

    // 3. Show another simple window.
    {
      ImGui::Begin(RIGHT); // Pass a pointer to our bool variable (the
                           // window will have a closing button that will
                           // clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      ImGui::End();
    }

    {
      // View
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
      if (ImGui::Begin(CENTER, nullptr,
                       ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoScrollWithMouse)) {

        if (ImGui::IsWindowFocused()) {
          // Input(ImGui::GetIO().KeysDown, input);
        }

        auto size = ImGui::GetContentRegionAvail();
        auto pos = ImGui::GetWindowPos();
        auto frameHeight = ImGui::GetFrameHeight();

        auto renderTarget =
            render(static_cast<int>(size.x), static_cast<int>(size.y));
        if (renderTarget) {
          ImGui::ImageButton((ImTextureID)renderTarget, size,
                             ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), 0);
        }
      }
      ImGui::End();
      ImGui::PopStyleVar();
    }

    // Rendering
    ImGui::Render();

    clear_color_with_alpha[0] = _clear_color.x * _clear_color.w;
    clear_color_with_alpha[1] = _clear_color.y * _clear_color.w;
    clear_color_with_alpha[2] = _clear_color.z * _clear_color.w;
    clear_color_with_alpha[3] = _clear_color.w;

    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  }
};

class Renderer {

  ComPtr<ID3D11Device> _device;
  ComPtr<ID3D11Texture2D> _texture;
  ComPtr<ID3D11ShaderResourceView> _srv;
  D3D11_TEXTURE2D_DESC _desc = {0};

  NvimFrontend &_nvim;
  NvimRendererD2D _renderer;

public:
  Renderer(NvimFrontend &nvim, const ComPtr<ID3D11Device> &device)
      : _nvim(nvim), _device(device),
        _renderer(device.Get(), nvim.DefaultAttribute()) {

    // Attach the renderer now that the window size is determined
    // auto [window_width, window_height] = window.Size();
    auto [font_width, font_height] = _renderer.FontSize();
    auto gridSize = Nvim::GridSize::FromWindowSize(640, 640, ceilf(font_width),
                                                   ceilf(font_height));

    // nvim_attach_ui. start redraw message
    _nvim.AttachUI(&_renderer, gridSize.rows, gridSize.cols);
  }

  ID3D11ShaderResourceView *Render(int w, int h) {
    // update target size
    GetOrCreate(w, h);

    // update nvim gird size
    auto [font_width, font_height] = _renderer.FontSize();
    auto gridSize = Nvim::GridSize::FromWindowSize(w, h, ceilf(font_width),
                                                   ceilf(font_height));
    if (_nvim.Sizing()) {
      auto a = 0;
    } else {
      if (_nvim.GridSize() != gridSize) {
        _nvim.SetSizing();
        _nvim.ResizeGrid(gridSize.rows, gridSize.cols);
      }
    }

    ComPtr<IDXGISurface2> surface;
    auto hr = _texture.As(&surface);
    assert(SUCCEEDED(hr));
    _renderer.SetTarget(surface.Get());
    _nvim.Process();
    _renderer.SetTarget(nullptr);

    return _srv.Get();
  }

  void Input(const Nvim::InputEvent &e) { _nvim.Input(e); }

private:
  void GetOrCreate(int w, int h) {
    if (_texture) {
      if (_desc.Width == w && _desc.Height == h) {
        return;
      }
    }

    PLOGD << "srv: " << w << ", " << h;

    _desc.Width = w;
    _desc.Height = h;
    _desc.MipLevels = 1;
    _desc.ArraySize = 1;
    _desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // D2D
    _desc.SampleDesc.Count = 1;
    _desc.SampleDesc.Quality = 0;
    _desc.Usage = D3D11_USAGE_DEFAULT;
    _desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    auto hr = _device->CreateTexture2D(&_desc, nullptr, &_texture);
    if (FAILED(hr)) {
      assert(false);
      return;
    }

    hr = _device->CreateShaderResourceView(_texture.Get(), nullptr, &_srv);
    if (FAILED(hr)) {
      assert(false);
      return;
    }
  }
};

// Main code
int main(int, char **) {
  static plog::DebugOutputAppender<plog::TxtFormatter> debugOutputAppender;
  plog::init(plog::verbose, &debugOutputAppender);

  //
  // launch nvim
  //
  NvimFrontend nvim;

  //
  // create window
  //
  Win32Window window;
  auto hwnd = window.Create(&nvim);
  if (!hwnd) {
    return 1;
  }

  if (!nvim.Launch(L"nvim --embed", [hwnd]() {
        PLOGD << "nvim terminated";
        PostMessage(hwnd, WM_DESTROY, 0, 0);
      })) {
    return 3;
  }
  auto [font, size] = nvim.Initialize();

  // Show the window
  ::ShowWindow(hwnd, SW_SHOWDEFAULT);
  ::UpdateWindow(hwnd);

  D3DManager d3d;
  if (!d3d.Create(hwnd)) {
    return 2;
  }
  Renderer renderer(nvim, d3d._pd3dDevice);

  Gui gui(hwnd, d3d._pd3dDevice.Get(), d3d._pd3dDeviceContext.Get());

  // Main loop
  bool done = false;
  while (!done) {
    // Poll and handle messages (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application. Generally you may always pass all inputs
    // to dear imgui, and hide them from your application based on those two
    // flags.
    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
      // ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;

    RECT rect;
    GetClientRect(hwnd, &rect);
    d3d.PrepareBackbuffer(rect.right - rect.left, rect.bottom - rect.top,
                          gui.clear_color_with_alpha);

    gui.Render(std::bind(&Renderer::Render, &renderer, std::placeholders::_1,
                         std::placeholders::_2),
               std::bind(&Renderer::Input, &renderer, std::placeholders::_1));
    d3d.Present();
  }

  return 0;
}
