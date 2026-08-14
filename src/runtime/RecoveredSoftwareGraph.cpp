#include "RecoveredSoftwareGraph.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <new>
#include <string>
#include <vector>

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
SRecoveredWindowPresentation g_presentation = {};
RECT g_windowedRect = {};
bool g_windowedRectValid = false;
DWORD g_presentationError = ERROR_SUCCESS;
void (*g_previousClipUpdate)() = nullptr;
TRecoveredSoftwareWindowMessageHook g_windowMessageHook = nullptr;
std::vector<SRecoveredDisplayMode> g_displayModes;
SRecoveredWindowsPresentationState g_windowsPresentationState = {};
std::wstring g_displayRecoveryPath;
std::wstring g_exclusiveDevice;
bool g_displayMutationActive = false;

constexpr std::size_t kMaximumDisplayModes = 512u;
constexpr std::size_t kMaximumRecoveryBytes = 1024u;
const char kRecoveryHeader[] = "RR2NW-DISPLAY-RECOVERY-1\r\n";

bool IsFourByThree(int width, int height) {
  return width >= kScreenWidth && height >= kScreenHeight &&
         width <= 7680 && height <= 4320 &&
         static_cast<long long>(width) * 3 ==
             static_cast<long long>(height) * 4;
}

bool PresentationEquals(const SRecoveredWindowPresentation& left,
                        const SRecoveredWindowPresentation& right) {
  return left.mode == right.mode &&
         left.clientWidth == right.clientWidth &&
         left.clientHeight == right.clientHeight &&
         left.bitsPerPixel == right.bitsPerPixel &&
         left.displayFrequency == right.displayFrequency &&
         left.displayDevice == right.displayDevice;
}

std::string WideToUtf8(const std::wstring& value) {
  if (value.empty()) return std::string();
  const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                       value.data(),
                                       static_cast<int>(value.size()),
                                       nullptr, 0, nullptr, nullptr);
  if (size <= 0) return std::string();
  std::string result(static_cast<std::size_t>(size), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), &result[0], size,
                          nullptr, nullptr) != size)
    return std::string();
  return result;
}

std::wstring Utf8ToWide(const std::string& value) {
  if (value.empty()) return std::wstring();
  const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       value.data(),
                                       static_cast<int>(value.size()),
                                       nullptr, 0);
  if (size <= 0) return std::wstring();
  std::wstring result(static_cast<std::size_t>(size), L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                          static_cast<int>(value.size()), &result[0], size) !=
      size)
    return std::wstring();
  return result;
}

bool MonitorDeviceForWindow(std::wstring* device) {
  if (device == nullptr || _gr_hWnd == nullptr) return false;
  const HMONITOR monitor =
      MonitorFromWindow(_gr_hWnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEXW info = {};
  info.cbSize = sizeof(info);
  if (monitor == nullptr || GetMonitorInfoW(monitor, &info) == FALSE)
    return false;
  *device = info.szDevice;
  return !device->empty();
}

bool WriteRecoveryMarker(const std::wstring& device) {
  if (g_displayRecoveryPath.empty() || device.empty()) return false;
  const std::string utf8 = WideToUtf8(device);
  if (utf8.empty()) return false;
  const std::string body = std::string(kRecoveryHeader) + "device=" + utf8 +
                           "\r\n";
  const std::wstring temporary = g_displayRecoveryPath + L".tmp";
  HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = WriteFile(file, body.data(), static_cast<DWORD>(body.size()),
                            &written, nullptr) != FALSE &&
                  written == body.size() && FlushFileBuffers(file) != FALSE;
  CloseHandle(file);
  if (!ok || MoveFileExW(temporary.c_str(), g_displayRecoveryPath.c_str(),
                         MOVEFILE_REPLACE_EXISTING |
                             MOVEFILE_WRITE_THROUGH) == FALSE) {
    DeleteFileW(temporary.c_str());
    return false;
  }
  return true;
}

bool ReadRecoveryMarker(std::wstring* device) {
  if (device == nullptr || g_displayRecoveryPath.empty()) return false;
  HANDLE file = CreateFileW(g_displayRecoveryPath.c_str(), GENERIC_READ,
                            FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size = {};
  const bool bounded = GetFileSizeEx(file, &size) != FALSE &&
                       size.QuadPart > 0 &&
                       size.QuadPart <=
                           static_cast<LONGLONG>(kMaximumRecoveryBytes);
  std::string bytes;
  if (bounded) bytes.resize(static_cast<std::size_t>(size.QuadPart));
  DWORD read = 0;
  const bool readOk = bounded &&
      ReadFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &read,
               nullptr) != FALSE && read == bytes.size();
  CloseHandle(file);
  if (!readOk || bytes.compare(0, sizeof(kRecoveryHeader) - 1u,
                               kRecoveryHeader) != 0)
    return false;
  const std::size_t begin = bytes.find("device=");
  if (begin == std::string::npos) return false;
  const std::size_t valueBegin = begin + 7u;
  const std::size_t end = bytes.find_first_of("\r\n", valueBegin);
  const std::string encoded = bytes.substr(valueBegin, end - valueBegin);
  *device = Utf8ToWide(encoded);
  return !device->empty() && device->size() < CCHDEVICENAME;
}

bool RestoreDesktopMode(const std::wstring& device, bool deleteMarker) {
  if (device.empty()) return false;
  g_displayMutationActive = true;
  const LONG result = ChangeDisplaySettingsExW(device.c_str(), nullptr,
                                                nullptr, 0, nullptr);
  g_displayMutationActive = false;
  if (result != DISP_CHANGE_SUCCESSFUL) {
    g_presentationError = static_cast<DWORD>(result);
    return false;
  }
  ++g_windowsPresentationState.desktopRestores;
  if (deleteMarker && !g_displayRecoveryPath.empty())
    DeleteFileW(g_displayRecoveryPath.c_str());
  return true;
}

bool RestoreAllAttachedDesktopModes() {
  bool attempted = false;
  bool restored = true;
  for (DWORD index = 0; index < 32u; ++index) {
    DISPLAY_DEVICEW display = {};
    display.cb = sizeof(display);
    if (EnumDisplayDevicesW(nullptr, index, &display, 0) == FALSE) break;
    if ((display.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0)
      continue;
    attempted = true;
    g_displayMutationActive = true;
    const LONG result = ChangeDisplaySettingsExW(
        display.DeviceName, nullptr, nullptr, 0, nullptr);
    g_displayMutationActive = false;
    if (result == DISP_CHANGE_SUCCESSFUL) {
      ++g_windowsPresentationState.desktopRestores;
    } else {
      restored = false;
      g_presentationError = static_cast<DWORD>(result);
    }
  }
  return attempted && restored;
}

bool ApplyExclusiveDisplayMode(
    const SRecoveredWindowPresentation& presentation,
    bool writeMarker) {
  DEVMODEW mode = {};
  mode.dmSize = sizeof(mode);
  mode.dmPelsWidth = static_cast<DWORD>(presentation.clientWidth);
  mode.dmPelsHeight = static_cast<DWORD>(presentation.clientHeight);
  mode.dmBitsPerPel = static_cast<DWORD>(presentation.bitsPerPixel);
  mode.dmDisplayFrequency =
      static_cast<DWORD>(presentation.displayFrequency);
  mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
  if (presentation.displayFrequency > 1)
    mode.dmFields |= DM_DISPLAYFREQUENCY;
  if (writeMarker && !WriteRecoveryMarker(presentation.displayDevice)) {
    g_presentationError = ERROR_WRITE_FAULT;
    return false;
  }
  g_displayMutationActive = true;
  const LONG result = ChangeDisplaySettingsExW(
      presentation.displayDevice.c_str(), &mode, nullptr, CDS_FULLSCREEN,
      nullptr);
  g_displayMutationActive = false;
  if (result != DISP_CHANGE_SUCCESSFUL) {
    g_presentationError = static_cast<DWORD>(result);
    if (writeMarker && !g_displayRecoveryPath.empty())
      DeleteFileW(g_displayRecoveryPath.c_str());
    return false;
  }
  ++g_windowsPresentationState.exclusiveApplies;
  return true;
}

bool SetSoftwareWindowStyle(const SRecoveredWindowPresentation& presentation) {
  if (_gr_hWnd == nullptr) return false;
  if (presentation.mode == RECOVERED_WINDOW_MODE_BORDERLESS ||
      presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE) {
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    const HMONITOR monitor =
        MonitorFromWindow(_gr_hWnd, MONITOR_DEFAULTTONEAREST);
    if (monitor == nullptr || GetMonitorInfoW(monitor, &info) == FALSE) {
      g_presentationError = GetLastError();
      return false;
    }
    SetLastError(ERROR_SUCCESS);
    SetWindowLongPtrW(_gr_hWnd, GWL_STYLE,
                      WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
    if (GetLastError() != ERROR_SUCCESS) {
      g_presentationError = GetLastError();
      return false;
    }
    const int width = presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE
                          ? presentation.clientWidth
                          : info.rcMonitor.right - info.rcMonitor.left;
    const int height = presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE
                           ? presentation.clientHeight
                           : info.rcMonitor.bottom - info.rcMonitor.top;
    if (SetWindowPos(_gr_hWnd, HWND_TOP, info.rcMonitor.left,
                     info.rcMonitor.top, width, height,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW) == FALSE) {
      g_presentationError = GetLastError();
      return false;
    }
    return true;
  }

  const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                      WS_MINIMIZEBOX | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
  SetLastError(ERROR_SUCCESS);
  SetWindowLongPtrW(_gr_hWnd, GWL_STYLE, style);
  if (GetLastError() != ERROR_SUCCESS) {
    g_presentationError = GetLastError();
    return false;
  }
  RECT outer = {0, 0, presentation.clientWidth, presentation.clientHeight};
  if (AdjustWindowRectEx(&outer, style, GetMenu(_gr_hWnd) != nullptr,
                         WS_EX_APPWINDOW) == FALSE) {
    g_presentationError = GetLastError();
    return false;
  }
  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  if (g_windowedRectValid) {
    x = g_windowedRect.left;
    y = g_windowedRect.top;
  }
  if (SetWindowPos(_gr_hWnd, HWND_NOTOPMOST, x, y,
                   outer.right - outer.left, outer.bottom - outer.top,
                   SWP_FRAMECHANGED | SWP_SHOWWINDOW) == FALSE) {
    g_presentationError = GetLastError();
    return false;
  }
  if (GetWindowRect(_gr_hWnd, &g_windowedRect) != FALSE)
    g_windowedRectValid = true;
  return true;
}

void MakeProcessDpiAware() {
  HMODULE user32 = GetModuleHandleW(L"user32.dll");
  using SetDpiAwarenessContext = BOOL(WINAPI*)(HANDLE);
  SetDpiAwarenessContext setContext = user32 == nullptr
      ? nullptr
      : reinterpret_cast<SetDpiAwarenessContext>(
            GetProcAddress(user32, "SetProcessDpiAwarenessContext"));
  if (setContext != nullptr &&
      setContext(reinterpret_cast<HANDLE>(-4)) != FALSE) {
    g_windowsPresentationState.dpiAware = true;
    return;
  }
  g_windowsPresentationState.dpiAware = SetProcessDPIAware() != FALSE ||
      GetLastError() == ERROR_ACCESS_DENIED;
}

void HandlePresentationWindowMessage(UINT message, WPARAM wParam,
                                     LPARAM lParam) {
  if (message == WM_ACTIVATEAPP &&
      g_presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE &&
      !g_displayMutationActive) {
    if (wParam == FALSE && !g_windowsPresentationState.exclusiveSuspended) {
      if (RestoreDesktopMode(g_exclusiveDevice, false)) {
        g_windowsPresentationState.exclusiveSuspended = true;
        g_windowsPresentationState.exclusiveActive = false;
        ++g_windowsPresentationState.focusSuspends;
      }
    } else if (wParam != FALSE &&
               g_windowsPresentationState.exclusiveSuspended) {
      if (ApplyExclusiveDisplayMode(g_presentation, false) &&
          SetSoftwareWindowStyle(g_presentation)) {
        g_windowsPresentationState.exclusiveSuspended = false;
        g_windowsPresentationState.exclusiveActive = true;
        ++g_windowsPresentationState.focusResumes;
      } else {
        const SRecoveredWindowPresentation fallback = {};
        (void)RestoreDesktopMode(g_exclusiveDevice, true);
        g_exclusiveDevice.clear();
        g_windowsPresentationState.exclusiveSuspended = false;
        g_windowsPresentationState.exclusiveActive = false;
        ++g_windowsPresentationState.focusFallbacks;
        (void)SetSoftwareWindowStyle(fallback);
        g_presentation = fallback;
      }
    }
  } else if (message == WM_DPICHANGED &&
             g_presentation.mode == RECOVERED_WINDOW_MODE_WINDOWED &&
             lParam != 0) {
    const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
    if (SetWindowPos(_gr_hWnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE) != FALSE)
      ++g_windowsPresentationState.dpiChanges;
  } else if (message == WM_DISPLAYCHANGE && !g_displayMutationActive) {
    ++g_windowsPresentationState.displayChanges;
  }
}

void UpdateSoftwareClip() {}

LRESULT CALLBACK SoftwareWindowProc(HWND window, UINT message, WPARAM wParam,
                                    LPARAM lParam) {
  HandlePresentationWindowMessage(message, wParam, lParam);
  switch (message) {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT: {
      PAINTSTRUCT paint = {};
      BeginPaint(window, &paint);
      (void)GRDumpScreen();
      EndPaint(window, &paint);
      return 0;
    }
    case WM_CLOSE:
      DestroyWindow(window);
      return 0;
    case WM_DESTROY:
      if (!g_programmaticWindowDestroy) PostQuitMessage(0);
      return 0;
    default:
      if (g_windowMessageHook != nullptr) {
        return g_windowMessageHook(window, message, wParam, lParam);
      }
      return DefWindowProcA(window, message, wParam, lParam);
  }
}

bool CreateSoftwareWindow(HINSTANCE instance) {
  if (instance == nullptr) return true;
  g_windowInstance = instance;
  MakeProcessDpiAware();

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

  if (GetWindowRect(_gr_hWnd, &g_windowedRect) != FALSE)
    g_windowedRectValid = true;

  _gr_hDC = GetDC(_gr_hWnd);
  if (_gr_hDC == nullptr) return false;

  ShowWindow(_gr_hWnd, SW_SHOWNORMAL);
  UpdateWindow(_gr_hWnd);
  return true;
}

void FinishSoftwareGraph() {
  if (!g_exclusiveDevice.empty()) {
    (void)RestoreDesktopMode(g_exclusiveDevice, true);
    g_exclusiveDevice.clear();
  }
  g_windowsPresentationState.exclusiveActive = false;
  g_windowsPresentationState.exclusiveSuspended = false;
  RecoveredLevelRuntime_Release();
  GRSoftwareClearDitherTable();
  SetMixLightTable(nullptr);

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
  g_windowMessageHook = nullptr;
  g_ready = false;
  g_presentation = {};
  g_windowedRect = {};
  g_windowedRectValid = false;
  g_presentationError = ERROR_SUCCESS;
  g_displayModes.clear();
  g_displayRecoveryPath.clear();
  g_windowsPresentationState = {};
}

}  // namespace

int RecoveredSoftwareGraph_Initialize(HINSTANCE instance) {
  if (g_ready) return TRUE;
  if (_dL.currDevice != nullptr || _gr_pScreen != nullptr ||
      ppViewports != nullptr) {
    return FALSE;
  }

  GRSoftwareResetTotalStats();
  GRSoftwareResetPresentStats();

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

bool RecoveredSoftwareGraph_ValidatePresentation(
    const SRecoveredWindowPresentation& presentation) {
  if (presentation.mode != RECOVERED_WINDOW_MODE_WINDOWED &&
      presentation.mode != RECOVERED_WINDOW_MODE_BORDERLESS &&
      presentation.mode != RECOVERED_WINDOW_MODE_EXCLUSIVE)
    return false;
  if (presentation.clientWidth < kScreenWidth ||
      presentation.clientHeight < kScreenHeight ||
      presentation.clientWidth > 7680 || presentation.clientHeight > 4320)
    return false;
  // This slice deliberately scales the retail 4:3 software framebuffer. It
  // does not lie to the scene/panel ABI about an arbitrary internal mode.
  if (!IsFourByThree(presentation.clientWidth, presentation.clientHeight))
    return false;
  if (presentation.mode != RECOVERED_WINDOW_MODE_EXCLUSIVE)
    return true;
  return presentation.bitsPerPixel >= 16 &&
         presentation.bitsPerPixel <= 64 &&
         presentation.displayFrequency >= 1 &&
         presentation.displayFrequency <= 1000 &&
         !presentation.displayDevice.empty() &&
         presentation.displayDevice.size() < CCHDEVICENAME;
}

bool RecoveredSoftwareGraph_ApplyPresentation(
    const SRecoveredWindowPresentation& presentation,
    SRecoveredWindowPresentation* previous) {
  g_presentationError = ERROR_SUCCESS;
  if (!g_ready || _gr_hWnd == nullptr) {
    g_presentationError = ERROR_INVALID_STATE;
    return false;
  }
  if (!RecoveredSoftwareGraph_ValidatePresentation(presentation)) {
    g_presentationError = ERROR_INVALID_PARAMETER;
    return false;
  }
  if (previous != nullptr) *previous = g_presentation;
  if (PresentationEquals(presentation, g_presentation)) return true;

  const SRecoveredWindowPresentation oldPresentation = g_presentation;
  const ERecoveredWindowMode oldMode = g_presentation.mode;
  if (oldMode == RECOVERED_WINDOW_MODE_WINDOWED &&
      presentation.mode != RECOVERED_WINDOW_MODE_WINDOWED &&
      GetWindowRect(_gr_hWnd, &g_windowedRect) != FALSE)
    g_windowedRectValid = true;
  if (oldMode == RECOVERED_WINDOW_MODE_EXCLUSIVE &&
      !g_exclusiveDevice.empty() &&
      !RestoreDesktopMode(g_exclusiveDevice, true))
    return false;
  g_windowsPresentationState.exclusiveActive = false;
  g_windowsPresentationState.exclusiveSuspended = false;
  g_exclusiveDevice.clear();

  const auto restoreOldPresentation = [&oldPresentation, oldMode]() {
    if (oldMode == RECOVERED_WINDOW_MODE_EXCLUSIVE) {
      if (!ApplyExclusiveDisplayMode(oldPresentation, true)) return false;
      g_exclusiveDevice = oldPresentation.displayDevice;
      g_windowsPresentationState.exclusiveActive = true;
    }
    return SetSoftwareWindowStyle(oldPresentation);
  };

  if (presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE) {
    if (!ApplyExclusiveDisplayMode(presentation, true)) {
      const DWORD failure = g_presentationError;
      (void)restoreOldPresentation();
      g_presentationError = failure;
      return false;
    }
    g_exclusiveDevice = presentation.displayDevice;
    g_windowsPresentationState.exclusiveActive = true;
  }
  if (!SetSoftwareWindowStyle(presentation)) {
    const DWORD failure = g_presentationError;
    if (presentation.mode == RECOVERED_WINDOW_MODE_EXCLUSIVE) {
      (void)RestoreDesktopMode(presentation.displayDevice, true);
      g_exclusiveDevice.clear();
      g_windowsPresentationState.exclusiveActive = false;
    }
    (void)restoreOldPresentation();
    g_presentationError = failure;
    return false;
  }
  g_presentation = presentation;
  if (presentation.mode != RECOVERED_WINDOW_MODE_EXCLUSIVE &&
      !g_displayRecoveryPath.empty())
    DeleteFileW(g_displayRecoveryPath.c_str());
  InvalidateRect(_gr_hWnd, nullptr, FALSE);
  (void)RecoveredSoftwareGraph_RefreshDisplayModes();
  return true;
}

SRecoveredWindowPresentation RecoveredSoftwareGraph_Presentation() {
  return g_presentation;
}

unsigned long RecoveredSoftwareGraph_LastPresentationError() {
  return g_presentationError;
}

bool RecoveredSoftwareGraph_ConfigureDisplayRecovery(
    const std::wstring& recoveryPath) {
  if (recoveryPath.empty() ||
      recoveryPath.find(L'\0') != std::wstring::npos)
    return false;
  g_displayRecoveryPath = recoveryPath;
  g_windowsPresentationState.recoveryConfigured = true;
  std::wstring staleDevice;
  if (GetFileAttributesW(recoveryPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
    const bool markerValid = ReadRecoveryMarker(&staleDevice);
    if (!markerValid) ++g_windowsPresentationState.corruptRecoveryMarkers;
    if ((!markerValid || !RestoreDesktopMode(staleDevice, false)) &&
        !RestoreAllAttachedDesktopModes()) {
      ++g_windowsPresentationState.recoveryFailures;
      return false;
    }
    DeleteFileW(recoveryPath.c_str());
    g_presentationError = ERROR_SUCCESS;
    g_windowsPresentationState.staleModeRecovered = true;
  }
  return RecoveredSoftwareGraph_RefreshDisplayModes();
}

bool RecoveredSoftwareGraph_RefreshDisplayModes() {
  g_displayModes.clear();
  if (_gr_hWnd == nullptr) return true;
  std::wstring device;
  if (!MonitorDeviceForWindow(&device)) return false;
  DEVMODEW current = {};
  current.dmSize = sizeof(current);
  const bool haveCurrent = EnumDisplaySettingsExW(
      device.c_str(), ENUM_CURRENT_SETTINGS, &current, 0) != FALSE;
  for (DWORD index = 0; index < kMaximumDisplayModes; ++index) {
    DEVMODEW mode = {};
    mode.dmSize = sizeof(mode);
    if (EnumDisplaySettingsExW(device.c_str(), index, &mode, 0) == FALSE)
      break;
    const int width = static_cast<int>(mode.dmPelsWidth);
    const int height = static_cast<int>(mode.dmPelsHeight);
    const int bits = static_cast<int>(mode.dmBitsPerPel);
    const int frequency = static_cast<int>(mode.dmDisplayFrequency);
    if (!IsFourByThree(width, height) || bits < 16 || bits > 64 ||
        frequency < 1 || frequency > 1000)
      continue;
    const auto duplicate = std::find_if(
        g_displayModes.begin(), g_displayModes.end(),
        [width, height, bits, frequency](const SRecoveredDisplayMode& entry) {
          return entry.width == width && entry.height == height &&
                 entry.bitsPerPixel == bits &&
                 entry.displayFrequency == frequency;
        });
    if (duplicate != g_displayModes.end()) continue;
    SRecoveredDisplayMode entry;
    entry.width = width;
    entry.height = height;
    entry.bitsPerPixel = bits;
    entry.displayFrequency = frequency;
    entry.displayDevice = device;
    entry.current = haveCurrent && current.dmPelsWidth == mode.dmPelsWidth &&
                    current.dmPelsHeight == mode.dmPelsHeight &&
                    current.dmBitsPerPel == mode.dmBitsPerPel &&
                    current.dmDisplayFrequency == mode.dmDisplayFrequency;
    g_displayModes.push_back(entry);
  }
  std::sort(g_displayModes.begin(), g_displayModes.end(),
            [](const SRecoveredDisplayMode& left,
               const SRecoveredDisplayMode& right) {
              if (left.width != right.width) return left.width < right.width;
              if (left.height != right.height)
                return left.height < right.height;
              if (left.bitsPerPixel != right.bitsPerPixel)
                return left.bitsPerPixel > right.bitsPerPixel;
              return left.displayFrequency < right.displayFrequency;
            });
  ++g_windowsPresentationState.catalogRefreshes;
  // An empty filtered catalog disables only exclusive mode. Windowed and
  // borderless remain valid fail-safe presentations on unusual drivers.
  return true;
}

std::size_t RecoveredSoftwareGraph_DisplayModeCount() {
  return g_displayModes.size();
}

bool RecoveredSoftwareGraph_DisplayMode(
    std::size_t index, SRecoveredDisplayMode* mode) {
  if (mode == nullptr || index >= g_displayModes.size()) return false;
  *mode = g_displayModes[index];
  return true;
}

bool RecoveredSoftwareGraph_FindDisplayMode(
    int width, int height, int bitsPerPixel, int displayFrequency,
    std::size_t* index) {
  if (index == nullptr) return false;
  for (std::size_t candidate = 0; candidate < g_displayModes.size();
       ++candidate) {
    const SRecoveredDisplayMode& mode = g_displayModes[candidate];
    if (mode.width == width && mode.height == height &&
        mode.bitsPerPixel == bitsPerPixel &&
        mode.displayFrequency == displayFrequency) {
      *index = candidate;
      return true;
    }
  }
  return false;
}

SRecoveredWindowsPresentationState
RecoveredSoftwareGraph_WindowsPresentationState() {
  return g_windowsPresentationState;
}

void RecoveredSoftwareGraph_ConfigureWindowMessageHook(
    TRecoveredSoftwareWindowMessageHook hook) {
  g_windowMessageHook = hook;
}
