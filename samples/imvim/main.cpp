// Dear ImGui: standalone example application for DirectX 11
// If you are new to Dear ImGui, read documentation from the docs/ folder + read
// the top of imgui.cpp. Read online:
// https://github.com/ocornut/imgui/tree/master/docs

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <nvim_pipe.h>
#include <d3d11.h>
#include <tchar.h>
#include <wrl/client.h>

template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    return true;

  switch (msg) {
  case WM_SIZE:
    return 0;
  case WM_SYSCOMMAND:
    if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
      return 0;
    break;
  case WM_DESTROY:
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

class Win32Window {
  WNDCLASSEX _wc = {0};
  HWND _hwnd = nullptr;

public:
  ~Win32Window() {
    ::DestroyWindow(_hwnd);
    ::UnregisterClass(_wc.lpszClassName, _wc.hInstance);
  }
  HWND Create() {
    // Create application window
    // ImGui_ImplWin32_EnableDpiAwareness();
    _wc = {sizeof(WNDCLASSEX),    CS_CLASSDC, WndProc, 0L,   0L,
          GetModuleHandle(NULL), NULL,       NULL,    NULL, NULL,
          _T("ImGui Example"),   NULL};
    ::RegisterClassEx(&_wc);
    _hwnd = ::CreateWindow(_wc.lpszClassName, _T("Dear ImGui DirectX11 Example"),
                          WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL,
                          _wc.hInstance, NULL);

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

    UINT createDeviceFlags = 0;
    // createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
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

class Gui {
  // Our state
  bool _show_demo_window = true;
  bool _show_another_window = false;
  ImVec4 _clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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

  void Render() {
    // Start the Dear ImGui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 1. Show the big demo window (Most of the sample code is in
    // ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear
    // ImGui!).
    if (_show_demo_window)
      ImGui::ShowDemoWindow(&_show_demo_window);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair
    // to created a named window.
    {
      static float f = 0.0f;
      static int counter = 0;

      ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                     // and append into it.

      ImGui::Text("This is some useful text."); // Display some text (you can
                                                // use a format strings too)
      ImGui::Checkbox(
          "Demo Window",
          &_show_demo_window); // Edit bools storing our window open/close state
      ImGui::Checkbox("Another Window", &_show_another_window);

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
    if (_show_another_window) {
      ImGui::Begin(
          "Another Window",
          &_show_another_window); // Pass a pointer to our bool variable (the
                                 // window will have a closing button that will
                                 // clear the bool when clicked)
      ImGui::Text("Hello from another window!");
      if (ImGui::Button("Close Me"))
        _show_another_window = false;
      ImGui::End();
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

// Main code
int main(int, char **) {

  // launch nvim
  NvimPipe nvim;
  if(!nvim.Launch("nvim --embed"))
  {
    return 3;
  }

  Win32Window window;
  auto hwnd = window.Create();
  if (!hwnd) {
    return 1;
  }

  D3DManager d3d;
  if (!d3d.Create(hwnd)) {
    return 2;
  }

  // Show the window
  ::ShowWindow(hwnd, SW_SHOWDEFAULT);
  ::UpdateWindow(hwnd);

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
      ::TranslateMessage(&msg);
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

    gui.Render();
    d3d.Present();
  }

  return 0;
}
