#include "RecoveredSoftwareGraph.h"

#include <cstddef>
#include <cstring>
#include <new>

#define LAST_H__VIEW
#include "game.h"
#include "graph.h"

#include "RecoveredLevelRuntime.h"
#include "ZavShutdownState.h"

extern SDeviceList _dL;
extern unsigned char _currPalette[768];

namespace {

const char kWindowClass[] = "RR2NWRecoveredSoftwareWindow";
const int kScreenWidth = 640;
const int kScreenHeight = 480;

SDeviceDescr g_softwareDevice = {};
HINSTANCE g_windowInstance = nullptr;
bool g_windowClassOwned = false;
bool g_programmaticWindowDestroy = false;
bool g_ready = false;
void (*g_previousClipUpdate)() = nullptr;

void UpdateSoftwareClip() {}

LRESULT CALLBACK SoftwareWindowProc(HWND window, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_CLOSE:
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      if (!g_programmaticWindowDestroy) PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcA(window, message, wParam, lParam);
  }
}

bool CreateSoftwareWindow(HINSTANCE instance) {
  if (instance == nullptr) return true;
  g_windowInstance = instance;

  WNDCLASSEXA windowClass = {};
  windowClass.cbSize = sizeof(windowClass);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = SoftwareWindowProc;
  windowClass.hInstance = instance;
  windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
  windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  windowClass.lpszClassName = kWindowClass;

  if (RegisterClassExA(&windowClass) != 0) {
    g_windowClassOwned = true;
  } else if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    return false;
  }

  RECT windowRect = {0, 0, kScreenWidth, kScreenHeight};
  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  if (AdjustWindowRectEx(&windowRect, style, FALSE, WS_EX_APPWINDOW) == FALSE) {
    return false;
  }

  _gr_hWnd = CreateWindowExA(
      WS_EX_APPWINDOW, kWindowClass, "Russian Roulette II: The Next Worlds",
      style, CW_USEDEFAULT, CW_USEDEFAULT, windowRect.right - windowRect.left,
      windowRect.bottom - windowRect.top, nullptr, nullptr, instance, nullptr);
  if (_gr_hWnd == nullptr) return false;

  _gr_hDC = GetDC(_gr_hWnd);
  if (_gr_hDC == nullptr) return false;

  ShowWindow(_gr_hWnd, SW_SHOWNORMAL);
  UpdateWindow(_gr_hWnd);
  return true;
}

void FinishSoftwareGraph() {
  RecoveredLevelRuntime_Release();

  if (_gr_hDC != nullptr && _gr_hWnd != nullptr) {
    ReleaseDC(_gr_hWnd, _gr_hDC);
  }
  _gr_hDC = nullptr;

  if (_gr_hWnd != nullptr) {
    g_programmaticWindowDestroy = true;
    DestroyWindow(_gr_hWnd);
    g_programmaticWindowDestroy = false;
  }
  _gr_hWnd = nullptr;

  if (g_windowClassOwned && g_windowInstance != nullptr) {
    UnregisterClassA(kWindowClass, g_windowInstance);
  }
  g_windowClassOwned = false;
  g_windowInstance = nullptr;

  delete[] _gr_pScreen;
  _gr_pScreen = nullptr;
  _gr_pOrigin = nullptr;
  _gr_pYCache = nullptr;
  _gr_nScreenWidth = 320;
  _gr_nScreenHeight = 200;
  _gr_nScreenOriginX = 0;
  _gr_nScreenOriginY = 0;
  _gr_clipRect.left = 0;
  _gr_clipRect.top = 0;
  _gr_clipRect.right = 0;
  _gr_clipRect.bottom = 0;
  _gr_bRestoreSurf = 0;
  std::memset(&_gr_DIBInfo, 0, sizeof(_gr_DIBInfo));
  std::memset(&g_softwareDevice, 0, sizeof(g_softwareDevice));
  _dL = SDeviceList{};
  _pGRSetClipRect = g_previousClipUpdate;
  g_previousClipUpdate = nullptr;
  g_ready = false;
}

}  // namespace

int RecoveredSoftwareGraph_Initialize(HINSTANCE instance) {
  if (g_ready) return TRUE;
  if (_dL.currDevice != nullptr || _gr_pScreen != nullptr ||
      ppViewports != nullptr) {
    return FALSE;
  }

  std::memset(&g_softwareDevice, 0, sizeof(g_softwareDevice));
  std::strcpy(g_softwareDevice.name, "RR2NW software DIB");
  g_softwareDevice.swHw = GR_SOFTWARE;
  g_softwareDevice.present = TRUE;
  g_softwareDevice.textureW = 256;
  g_softwareDevice.textureH = 256;
  g_softwareDevice.textureFormat[NORMAL_TEXTURE_INDEX].rgbBitCount = 8;
  g_softwareDevice.modesQnty = 1;
  g_softwareDevice.modes[0] = MAKE_MODE_DATA(kScreenWidth, kScreenHeight, 8);

  _dL.currDevice = &g_softwareDevice;
  _dL.currMode = 0;
  _dL.chooseDevice = &g_softwareDevice;
  _dL.chooseMode = 0;
  _dL.chooseFullScreen = FALSE;
  _dL.dDescr = &g_softwareDevice;
  _gr_nScreenWidth = kScreenWidth;
  _gr_nScreenHeight = kScreenHeight;

  const std::size_t pixelCount =
      static_cast<std::size_t>(kScreenWidth) * kScreenHeight;
  _gr_pScreen = new (std::nothrow) unsigned char[pixelCount];
  if (_gr_pScreen == nullptr) {
    FinishSoftwareGraph();
    return FALSE;
  }
  std::memset(_gr_pScreen, 0, pixelCount);

  _gr_DIBInfo.bmiHeader.biSize = sizeof(_gr_DIBInfo.bmiHeader);
  _gr_DIBInfo.bmiHeader.biWidth = kScreenWidth;
  _gr_DIBInfo.bmiHeader.biHeight = -kScreenHeight;
  _gr_DIBInfo.bmiHeader.biPlanes = 1;
  _gr_DIBInfo.bmiHeader.biBitCount = 8;
  _gr_DIBInfo.bmiHeader.biCompression = BI_RGB;
  _gr_DIBInfo.bmiHeader.biClrUsed = 256;
  _gr_DIBInfo.bmiHeader.biClrImportant = 0;

  g_previousClipUpdate = _pGRSetClipRect;
  _pGRSetClipRect = UpdateSoftwareClip;
  if (!CreateSoftwareWindow(instance)) {
    FinishSoftwareGraph();
    return FALSE;
  }

  (void)GRSetPalette(_currPalette, FALSE);
  CRect2 viewRect(0, 0, kScreenWidth, kScreenHeight);
  SGRViewport* viewport =
      GRCreateViewport(kScreenWidth / 2, kScreenHeight / 2, viewRect);
  if (viewport == nullptr) {
    FinishSoftwareGraph();
    return FALSE;
  }

  ppViewports = new (std::nothrow) SGRViewport*[1];
  if (ppViewports == nullptr) {
    GRReleaseViewport(viewport);
    FinishSoftwareGraph();
    return FALSE;
  }
  ppViewports[0] = viewport;
  CViewObject::SetClipRect(viewport->clipRect);
  GRSetViewport(viewport);

  const SZavShutdownHooks shutdownHooks = {
      nullptr, nullptr, GRReleaseViewport, nullptr, FinishSoftwareGraph};
  ZAV_ConfigureShutdown(shutdownHooks);
  ZAV_ArmGraphShutdown(1);
  g_ready = true;
  return TRUE;
}

bool RecoveredSoftwareGraph_IsReady() { return g_ready; }

int RecoveredSoftwareGraph_Width() {
  return g_ready ? kScreenWidth : 0;
}

int RecoveredSoftwareGraph_Height() {
  return g_ready ? kScreenHeight : 0;
}
